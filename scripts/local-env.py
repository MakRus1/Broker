#!/usr/bin/env python3
"""Изолированное локальное окружение; пользовательские БД и volumes не затрагиваются."""
import argparse
import json
import os
import secrets
import shutil
from pathlib import Path
import subprocess
import sys
import yaml
from service_catalog import discover as discover_services
from migrations import discover, render
sys.path.insert(0, str(Path(__file__).resolve().parent / 'openapi_gen'))
from generate import UniqueLoader
from dashboards import provision as provision_dashboards
from service_accounts import load as load_accounts, secret_key, variables as account_variables

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / ".local"


def resolve(value, variables):
    if isinstance(value, dict):
        return {key: resolve(item, variables) for key, item in value.items()}
    if isinstance(value, list):
        return [resolve(item, variables) for item in value]
    if isinstance(value, str) and value.startswith("$"):
        return variables[value[1:]]
    return value


def client_variables(name):
    # Адреса объявленных клиентов берём из service.yaml, чтобы новые зависимости
    # не уходили на localhost внутри контейнера.
    ports = {'configs': 8090, 'broker': 8080, 'shipments': 8100,
             'transportations': 8110, 'counterparties': 8120, 'organizations': 8130, 'transport_locations': 8140, 'contracts': 8150, 'finance': 8160}
    specification = yaml.safe_load((ROOT / f'services/{name}/service.yaml').read_text())
    result = {}
    for alias, client in specification.get('clients', {}).items():
        target = client['service']
        if target not in ports:
            raise ValueError(f'Для клиента {name}.{alias} не настроен локальный сервис {target}')
        port = 8092 if alias == 'config_admin' else ports[target]
        result[f'client-{alias}-base-url'] = f"http://{target.replace('_', '-')}:{port}"
    return result


def write_monitoring_targets(state, services):
    # Target строится из фактически подготовленного конфига каждого C++-сервиса,
    # а не отдельного списка базовых сервисов и портов.
    targets = []
    for name, service in sorted(services.items()):
        if 'tools/local/run.sh' not in service.get('command', []):
            continue
        config = yaml.safe_load((state / f'{name}.yaml').read_text())
        port = config['components_manager']['components']['server']['listener-monitor']['port']
        targets.append({'targets': [f'{name}:{port}'],
                        'labels': {'environment': 'local', 'service': name}})
    directory = state / 'monitoring'
    directory.mkdir(exist_ok=True)
    path = directory / 'prometheus-targets.json'
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(targets, indent=2) + '\n')
    temporary.replace(path)


def prepare():
    STATE.mkdir(exist_ok=True)
    provision_dashboards(ROOT, STATE / "grafana-dashboards", UniqueLoader)
    (ROOT / "build-local").mkdir(exist_ok=True)
    secrets_path = STATE / "auth-secrets.json"
    if not secrets_path.exists():
        values = {key: secrets.token_urlsafe(24) for key in [
            "local-author-secret", "local-reviewer-secret", "author-password", "reviewer-password",
            "admin-password", "postgres-password"]}
        descriptor = os.open(secrets_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w") as target:
            json.dump(values, target)
    auth_secrets = json.loads(secrets_path.read_text())
    accounts = load_accounts(ROOT)
    for account in accounts.values():
        key = secret_key(account)
        if key not in auth_secrets:
            auth_secrets[key] = secrets.token_urlsafe(32)
    secrets_path.write_text(json.dumps(auth_secrets))
    secrets_path.chmod(0o600)
    realm = (ROOT / "tools/local/keycloak/realm.template.json").read_text()
    for key, value in auth_secrets.items():
        realm = realm.replace("${" + key + "}", value)
    (STATE / "broker-realm.json").write_text(realm)
    # Bind-mount указывает на стабильный каталог вне checkout: rebase заменяет inode исходников.
    theme_source = ROOT / 'tools/keycloak/themes/broker'
    theme_target = STATE / 'keycloak-theme'
    theme_target.mkdir(exist_ok=True)
    for source_path in theme_source.rglob('*'):
        if source_path.is_file():
            destination = theme_target / source_path.relative_to(theme_source)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source_path, destination)
    for existing in theme_target.rglob('*'):
        if existing.is_file() and not (theme_source / existing.relative_to(theme_target)).is_file():
            existing.unlink()

    images = json.loads((ROOT / "tools/local/images.json").read_text())["images"]
    images["dev"] = json.loads((ROOT / "tools/container-images.json").read_text())["images"]["dev"]
    volumes = {
        name: {}
        for name in [
            "build",
            "venv",
            "logs",
            "cache",
            "loki",
            "tempo",
            "prometheus",
            "grafana",
            "collector",
        ]
    }
    source = f"{ROOT}:/workspaces/broker:ro"
    shared = [
        source,
        "build:/workspaces/broker/build-local",
        "venv:/workspaces/broker/.venv-gen",
        "logs:/logs",
        "cache:/cache",
        f"{STATE}:/local:ro",
    ]
    services = {
        "prepare": {
            "image": images["dev"],
            "user": "0",
            "working_dir": "/workspaces/broker",
            "volumes": shared,
            "command": ["bash", "tools/local/build.sh"],
            "environment": {
                "PIP_CACHE_DIR": "/cache/pip",
                "CCACHE_DIR": "/cache/ccache",
                "CCACHE_BASEDIR": "/workspaces/broker",
                "CCACHE_MAXSIZE": "512M",
                "CCACHE_COMPILERCHECK": "content",
            },
        }
    }
    for name, port, monitor in [("configs", 8090, 8092), ("broker", 8080, 8082), ("shipments", 8100, 8102), ("transportations", 8110, 8112), ("counterparties", 8120, 8122), ("organizations", 8130, 8132), ("transport_locations", 8140, 8142), ("contracts", 8150, 8152), ("finance", 8160, 8162)]:
        variables = yaml.safe_load((ROOT / f"services/{name}/configs/config_vars.yaml").read_text())
        variables.update(
            {
                "pg-connection": f"postgresql://testsuite:testsuite@postgres-{name.replace('_', '-')}:5432/{name}_db_1",
                "otlp-enabled": True,
                "otlp-endpoint": "collector:4317",
                "config-server-url": "http://configs:8090",
                "auth-jwks-url": "http://keycloak:8080/realms/broker/protocol/openid-connect/certs",
            }
        )
        variables.update({'server-port': port, 'monitor-port': monitor})
        if name == 'shipments':
            variables.update({
                'client-transportations-base-url': 'http://transportations:8110',
                'client-organizations-base-url': 'http://organizations:8130',
            })
        if name == 'organizations':
            variables.update({'users-token-url': 'http://keycloak:8080/realms/broker/protocol/openid-connect/token',
                'users-admin-url': 'http://keycloak:8080/admin/realms/broker'})
        variables.update(account_variables(accounts, name, auth_secrets, 'http://keycloak:8080/realms/broker/protocol/openid-connect/token'))
        variables['organization-access-url'] = 'http://organizations:8130/api/organizations/access'
        variables['platform-access-url'] = 'http://organizations:8130/api/platform/access'
        variables["config-updates-enabled"] = name != "configs"
        config = yaml.safe_load((ROOT / f"services/{name}/configs/static_config.yaml").read_text())

        variables.update(client_variables(name))
        config = resolve(config, variables)
        components = config["components_manager"]["components"]
        components["server"]["listener-monitor"]["address"] = "0.0.0.0"
        components["otlp-logger"]["sinks"] = {"logs": "default", "tracing": "both"}
        (STATE / f"{name}.yaml").write_text(yaml.safe_dump(config, sort_keys=False))
        (STATE / f"{name}.sql").write_text(
            render(discover(ROOT / f"services/{name}/postgresql/schemas/db_1"))
        )
        volumes[f"postgres-{name}"] = {}
        # Временный сервер initdb принимает Unix-сокет, но еще не готов для миграций по TCP.
        services[f"postgres-{name}"] = {
            "image": images["postgres"],
            "networks": {"default": {"aliases": [f"postgres-{name.replace('_', '-')}"]}},
            "environment": {
                "POSTGRES_USER": "testsuite",
                "POSTGRES_PASSWORD": "testsuite",
                "POSTGRES_DB": f"{name}_db_1",
            },
            "volumes": [f"postgres-{name}:/var/lib/postgresql/data"],
            "healthcheck": {
                "test": ["CMD-SHELL", f"pg_isready -h 127.0.0.1 -U testsuite -d {name}_db_1"],
                "interval": "2s",
                "timeout": "3s",
                "retries": 30,
            },
        }
        services[f"migrate-{name}"] = {
            "image": images["postgres"],
            "environment": {"PGPASSWORD": "testsuite"},
            "volumes": [f"{STATE}:/local:ro"],
            "command": [
                "psql",
                "-h",
                f"postgres-{name}",
                "-U",
                "testsuite",
                "-d",
                f"{name}_db_1",
                "-f",
                f"/local/{name}.sql",
            ],
            "depends_on": {f"postgres-{name}": {"condition": "service_healthy"}},
        }
        services[name] = {
            "image": images["dev"],
            "user": "1001:1001",
            "working_dir": "/workspaces/broker",
            "volumes": shared,
            "command": ["bash", "tools/local/run.sh", name, f"build-local/services/{name}/{name}"],
            "networks": {"default": {"aliases": [name.replace('_', '-')]}},
            "ports": [f"127.0.0.1:{port+10000}:{port}", f"127.0.0.1:{monitor+10000}:{monitor}"],
            "depends_on": {
                "prepare": {"condition": "service_completed_successfully"},
                f"migrate-{name}": {"condition": "service_completed_successfully"},
            },
        }
    services['transport-locations-transfer'] = {
        'image': images['dev'], 'working_dir': '/workspaces/broker',
        'volumes': shared, 'environment': {'PGPASSWORD': 'testsuite'},
        'command': ['python3', 'scripts/transport_location_transfer.py'],
        'depends_on': {f'migrate-{name}': {'condition': 'service_completed_successfully'}
                       for name in ('shipments', 'transportations', 'transport_locations')},
    }
    for consumer in ('shipments', 'transportations', 'transport_locations'):
        services[consumer]['depends_on']['transport-locations-transfer'] = {'condition': 'service_completed_successfully'}
    # Ключ DaData хранится отдельно от генерируемых конфигов и не обязателен.
    services['counterparties']['env_file'] = [
        {'path': str(STATE / 'dadata.env'), 'required': False}
    ]
    volumes["postgres-keycloak"] = {}
    services["postgres-keycloak"] = {
        "image": images["postgres"],
        "environment": {"POSTGRES_USER": "keycloak", "POSTGRES_DB": "keycloak",
                        "POSTGRES_PASSWORD": auth_secrets["postgres-password"]},
        "volumes": ["postgres-keycloak:/var/lib/postgresql/data"],
        "healthcheck": {"test": ["CMD", "pg_isready", "-h", "127.0.0.1", "-U", "keycloak"],
                        "interval": "2s", "timeout": "3s", "retries": 30},
    }
    services["keycloak"] = {
        "image": images["keycloak"],
        "command": ["start-dev", "--import-realm"],
        "environment": {
            "KC_DB": "postgres", "KC_DB_URL": "jdbc:postgresql://postgres-keycloak:5432/keycloak",
            "KC_DB_USERNAME": "keycloak", "KC_DB_PASSWORD": auth_secrets["postgres-password"],
            "KC_HOSTNAME": "http://localhost:18888", "KC_HOSTNAME_STRICT": "false",
            "KC_BOOTSTRAP_ADMIN_USERNAME": "admin",
            "KC_BOOTSTRAP_ADMIN_PASSWORD": auth_secrets["admin-password"],
        },
        "ports": ["127.0.0.1:18888:8080"],
        "volumes": [f"{STATE}/broker-realm.json:/opt/keycloak/data/import/broker-realm.json:ro", f"{STATE}/keycloak-theme:/opt/keycloak/themes/broker:ro"],
        "depends_on": {"postgres-keycloak": {"condition": "service_healthy"}},
    }
    services["configs"]["depends_on"]["keycloak"] = {"condition": "service_started"}
    volumes["admin-ui"] = {}
    volumes["admin-node"] = {}
    services["admin-ui-build"] = {
        "image": images["node"], "working_dir": "/workspaces/broker",
        "volumes": [source, "admin-node:/node-cache", "admin-ui:/admin-ui"],
        "command": ["bash", "tools/local/build-admin.sh"],
    }
    services["keycloak-setup"] = {
        "image": images["dev"], "working_dir": "/workspaces/broker", "volumes": shared,
        "command": ["python3", "tools/local/configure_keycloak.py"],
        "depends_on": {"keycloak": {"condition": "service_started"}},
    }
    variables = yaml.safe_load((ROOT / "services/admin_bff/configs/config_vars.yaml").read_text())
    variables.update({
        "client-config_admin-base-url": "http://configs:8092",
        "client-organizations-base-url": "http://organizations:8130",
        "auth-jwks-url": "http://keycloak:8080/realms/broker/protocol/openid-connect/certs",
        "admin-token-url": "http://keycloak:8080/realms/broker/protocol/openid-connect/token",
        "admin-revoke-url": "http://keycloak:8080/realms/broker/protocol/openid-connect/revoke",
        "admin-assets-dir": "/admin-ui",
        "config-updates-enabled": True,
        "config-server-url": "http://configs:8090",
        "otlp-enabled": True,
        "otlp-endpoint": "collector:4317",
    })
    variables.update(account_variables(accounts, 'admin_bff', auth_secrets, 'http://keycloak:8080/realms/broker/protocol/openid-connect/token'))
    variables.update(client_variables('admin_bff'))
    config = resolve(yaml.safe_load((ROOT / "services/admin_bff/configs/static_config.yaml").read_text()), variables)
    config["components_manager"]["components"]["server"]["listener"]["port"] = 8180
    admin_components = config["components_manager"]["components"]
    admin_components["server"]["listener-monitor"]["address"] = "0.0.0.0"
    admin_components["otlp-logger"]["sinks"] = {"logs": "default", "tracing": "both"}
    (STATE / "admin_bff.yaml").write_text(yaml.safe_dump(config, sort_keys=False))
    services["admin_bff"] = {
        "image": images["dev"], "user": "1001:1001", "working_dir": "/workspaces/broker",
        "volumes": [*shared, "admin-ui:/admin-ui:ro"],
        "command": ["bash", "tools/local/run.sh", "admin_bff", "build-local/services/admin_bff/admin_bff"],
        "ports": ["127.0.0.1:13080:8180"],
        "depends_on": {
            "prepare": {"condition": "service_completed_successfully"},
            "admin-ui-build": {"condition": "service_completed_successfully"},
            "keycloak-setup": {"condition": "service_completed_successfully"},
            "configs": {"condition": "service_healthy"},
        },
        "healthcheck": {"test": ["CMD", "python3", "-c", "import urllib.request; urllib.request.urlopen('http://localhost:8182/ping/ready', timeout=2)"],
                        "interval": "3s", "timeout": "3s", "retries": 30},
    }
    volumes['web-ui'] = {}
    volumes['web-node'] = {}
    services['web-ui-build'] = {
        'image': images['node'], 'working_dir': '/workspaces/broker',
        'volumes': [source, 'web-node:/node-cache', 'web-ui:/web-ui'],
        'command': ['bash', 'tools/local/build-admin.sh', 'web', '/web-ui'],
    }
    services['logistics-setup'] = {
        'image': images['dev'], 'working_dir': '/workspaces/broker', 'volumes': shared,
        'command': ['python3', 'tools/local/seed_logistics.py'],
        'depends_on': {name: {'condition': 'service_completed_successfully'} for name in
                       ('keycloak-setup', 'migrate-shipments', 'migrate-transportations', 'migrate-counterparties', 'migrate-organizations')},
    }
    variables = yaml.safe_load((ROOT / 'services/web_bff/configs/config_vars.yaml').read_text())
    variables.update({
        'server-port': 8280, 'monitor-port': 8282,
        'client-organizations-base-url': 'http://organizations:8130',
        'client-counterparties-base-url': 'http://counterparties:8120',
        'client-shipments-base-url': 'http://shipments:8100',
        'client-transportations-base-url': 'http://transportations:8110',
        'auth-jwks-url': 'http://keycloak:8080/realms/broker/protocol/openid-connect/certs',
        'web-token-url': 'http://keycloak:8080/realms/broker/protocol/openid-connect/token',
        'web-revoke-url': 'http://keycloak:8080/realms/broker/protocol/openid-connect/revoke',
        'web-assets-dir': '/web-ui', 'config-updates-enabled': True,
        'config-server-url': 'http://configs:8090', 'otlp-enabled': True,
        'otlp-endpoint': 'collector:4317',
    })
    variables.update(client_variables('web_bff'))
    config = resolve(yaml.safe_load((ROOT / 'services/web_bff/configs/static_config.yaml').read_text()), variables)
    config['components_manager']['components']['server']['listener-monitor']['address'] = '0.0.0.0'
    (STATE / 'web_bff.yaml').write_text(yaml.safe_dump(config, sort_keys=False))
    services['web_bff'] = {
        'image': images['dev'], 'user': '1001:1001', 'working_dir': '/workspaces/broker',
        'volumes': [*shared, 'web-ui:/web-ui:ro'],
        'command': ['bash', 'tools/local/run.sh', 'web_bff', 'build-local/services/web_bff/web_bff'],
        'ports': ['127.0.0.1:14080:8280'],
        'env_file': [{'path': str(STATE / 'dadata.env'), 'required': False}],
        'depends_on': {'prepare': {'condition': 'service_completed_successfully'},
                       'web-ui-build': {'condition': 'service_completed_successfully'},
                       'logistics-setup': {'condition': 'service_completed_successfully'},
                       'counterparties': {'condition': 'service_started'},
                       'shipments': {'condition': 'service_started'},
                       'transportations': {'condition': 'service_started'},
                       'configs': {'condition': 'service_started'}},
    }
    for name, port, internal, args in [
        ("loki", 13100, 3100, ["-config.file=/etc/local/loki.yaml"]),
        ("tempo", 13200, 3200, ["-config.file=/etc/local/tempo.yaml"]),
        (
            "prometheus",
            19090,
            9090,
            [
                "--config.file=/etc/local/prometheus.yaml",
                "--storage.tsdb.retention.time=24h",
                "--storage.tsdb.retention.size=512MB",
            ],
        ),
    ]:
        path = (
            "/prometheus"
            if name == "prometheus"
            else ("/var/tempo" if name == "tempo" else f"/{name}")
        )
        services[name] = {
            "image": images[name],
            "user": "0",
            "command": args,
            "volumes": [f"{ROOT}/tools/local:/etc/local:ro", f"{name}:{path}"],
            "ports": [f"127.0.0.1:{port}:{internal}"],
        }
    fixture = ROOT / "tests/fixtures/contract_service/configs"
    variables = yaml.safe_load((fixture / "config_vars.yaml").read_text())
    variables.update(
        {
            "client-broker-base-url": "http://broker:8080",
            "config-server-url": "http://configs:8090",
            "config-updates-enabled": True,
        }
    )
    config = resolve(yaml.safe_load((fixture / "static_config.yaml").read_text()), variables)
    components = config["components_manager"]["components"]
    components["server"]["listener-monitor"]["address"] = "0.0.0.0"
    broker_components = yaml.safe_load((STATE / "broker.yaml").read_text())["components_manager"][
        "components"
    ]
    for key in (
        "grpc-client-common",
        "grpc-otlp-factory",
        "otlp-logger",
        "grpc-client-logging",
        "handler-server-monitor",
        "tracing-manager-locator",
    ):
        components[key] = broker_components[key]
    components["otlp-logger"]["service-name"] = "diagnostics"
    components["platform-http-telemetry"]["service-name"] = "diagnostics"
    components["platform-http-telemetry"]["environment"] = "local"
    components["scenario-grpc-factory"] = {}
    components["local-scenario"] = {
        "path": "/scenario",
        "method": "POST",
        "task_processor": "main-task-processor",
        "request_body_size_log_limit": 0,
        "response_data_size_log_limit": 0,
    }
    (STATE / "diagnostics.yaml").write_text(yaml.safe_dump(config, sort_keys=False))
    services["diagnostics"] = {
        "image": images["dev"],
        "user": "1001:1001",
        "working_dir": "/workspaces/broker",
        "volumes": shared,
        "environment": {"BROKER_LOCAL_SCENARIO": "1"},
        "command": [
            "bash",
            "tools/local/run.sh",
            "diagnostics",
            "build-local/tests/fixtures/contract_service/contract_service",
        ],
        "ports": ["127.0.0.1:18084:8084", "127.0.0.1:18085:8085"],
        "depends_on": {"prepare": {"condition": "service_completed_successfully"}},
    }
    initial = {}
    for schema in sorted((ROOT / "schemas/configs").glob("*/*.yaml")):
        document = yaml.safe_load(schema.read_text())
        value = document["default"]
        if document["mode"] == "conditional":
            value = {"clauses": [], "default": value}
        initial[document["name"]] = value
    (STATE / "initial-configs.json").write_text(json.dumps(initial))
    services["seed"] = {
        "image": images["dev"],
        "working_dir": "/workspaces/broker",
        "volumes": shared,
        "command": ["python3", "tools/local/seed.py"],
        "environment": {"BROKER_AUTH_TOKEN_URL": "http://keycloak:8080/realms/broker/protocol/openid-connect/token"},
        "depends_on": {"configs": {"condition": "service_healthy"}},
    }
    services["broker"]["depends_on"]["seed"] = {"condition": "service_completed_successfully"}
    services["diagnostics"]["depends_on"]["broker"] = {"condition": "service_healthy"}
    for name, port in [("broker", 8082), ("configs", 8092), ("diagnostics", 8085)]:
        services[name]["healthcheck"] = {
            "test": [
                "CMD",
                "python3",
                "-c",
                f"import urllib.request; urllib.request.urlopen('http://localhost:{port}/ping/ready', timeout=2)",
            ],
            "interval": "3s",
            "timeout": "3s",
            "retries": 30,
        }
    services["readiness"] = {
        "image": images["dev"],
        "working_dir": "/workspaces/broker",
        "volumes": [source],
        "command": ["python3", "tools/local/readiness.py"],
    }
    services["collector"] = {
        "image": images["collector"],
        "user": "0",
        "command": ["--config=/etc/local/collector.yaml"],
        "volumes": [f"{ROOT}/tools/local:/etc/local:ro", "logs:/logs:ro", "collector:/state"],
    }
    services["grafana"] = {
        "image": images["grafana"],
        "environment": {
            "GF_PLUGINS_PREINSTALL_DISABLED": "true",
            "GF_PLUGINS_PREINSTALL_AUTO_UPDATE": "false",
            "GF_ANALYTICS_CHECK_FOR_UPDATES": "false",
            "GF_ANALYTICS_REPORTING_ENABLED": "false",
        },
        "volumes": [
            f"{ROOT}/tools/local/grafana/datasources:/etc/grafana/provisioning/datasources:ro",
            f"{ROOT}/tools/local/grafana/dashboards:/etc/grafana/provisioning/dashboards:ro",
            "grafana:/var/lib/grafana",
            f"{STATE}/grafana-dashboards:/var/lib/grafana/dashboards:ro",
        ],
        "ports": ["127.0.0.1:13000:3000"],
    }
    for owner in {account['service'] for account in accounts.values()}:
        services[owner]['depends_on']['keycloak-setup'] = {'condition': 'service_completed_successfully'}

    for name, service in services.items():
        service["logging"] = {
            "driver": "json-file",
            "options": {"max-size": "10m", "max-file": "3"},
        }
        service["mem_limit"] = (
            "4g"
            if name == "prepare"
            else ("1g" if name in {"broker", "configs", "admin_bff", "admin-ui-build", "diagnostics", "tempo", "loki", "keycloak"} else "512m")
        )
    write_monitoring_targets(STATE, services)
    services['prometheus']['volumes'].append(f"{STATE}/monitoring:/etc/broker-local:ro")
    (STATE / "compose.yaml").write_text(
        yaml.safe_dump(
            {"name": "broker-local", "services": services, "volumes": volumes}, sort_keys=False
        )
    )


def update(service, jobs):
    if service not in discover_services(ROOT):
        raise ValueError(f'Неизвестный сервис: {service}')
    if jobs < 1:
        raise ValueError('Количество задач сборки должно быть положительным')
    compose = STATE / 'compose.yaml'
    if not compose.is_file():
        raise ValueError('Локальный стенд ещё не создан: выполните make local-up')
    command = ['docker', 'compose', '-f', str(compose)]
    container = subprocess.check_output(command + ['ps', '--all', '--quiet', service], text=True)
    if not container.strip():
        raise ValueError(f'Контейнер {service} отсутствует: сначала выполните make local-up')
    prepare()
    services = yaml.safe_load(compose.read_text())['services']
    if service not in services:
        raise ValueError(f'{service} не поддерживается локальным стендом')
    config = yaml.safe_load((STATE / f'{service}.yaml').read_text())
    port = config['components_manager']['components']['server']['listener-monitor']['port']
    print(f'Сборка только {service}, затем миграции и перезапуск этого сервиса', flush=True)
    subprocess.run(
        command
        + [
            'run',
            '--rm',
            '--no-deps',
            '-e',
            f'NPROCS={jobs}',
            'prepare',
            'bash',
            'tools/local/build.sh',
            service,
        ],
        check=True,
    )
    migration = f'migrate-{service}'
    if migration in services:
        print(f'Миграции {service}', flush=True)
        subprocess.run(command + ['run', '--rm', '--no-deps', migration], check=True)
    print(f'Перезапуск {service}', flush=True)
    try:
        subprocess.run(command + ['up', '-d', '--no-deps', '--force-recreate', service], check=True)
        subprocess.run(
            command
            + [
                'exec',
                '-T',
                service,
                'python3',
                'tools/local/wait_ready.py',
                '--port',
                str(port),
                '--timeout',
                '120',
            ],
            check=True,
        )
    except subprocess.CalledProcessError:
        subprocess.run(command + ['logs', '--no-color', '--tail', '50', service], check=False)
        raise
    print(f'{service} готов', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["up", "down", "status", "logs", "check", "migrate", "update"])
    parser.add_argument('--service')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.action == 'update':
        if not args.service:
            parser.error('update требует --service; пример: make local-update SERVICE=shipments')
        update(args.service, args.jobs)
        return
    if args.service:
        parser.error('--service поддерживается только для update')
    prepare()
    command = ["docker", "compose", "-f", str(STATE / "compose.yaml")]
    actions = {
        "up": ["up", "-d", "--force-recreate", "--wait", "--wait-timeout", "1800"],
        "down": ["down"],
        "status": ["ps", "-a"],
        "logs": ["logs", "--tail=100"],
        "migrate": ["up", "--force-recreate", "migrate-broker", "migrate-configs"],
        "check": ["ps", "-a"],
    }
    if args.action == "check":
        subprocess.run([sys.executable, str(ROOT / "tools/local/check_auth.py")], check=True)
        subprocess.run([sys.executable, str(ROOT / "tools/local/check.py")], check=True)
        subprocess.run([sys.executable, str(ROOT / "tools/local/check_logs.py")], check=True)
        subprocess.run([sys.executable, str(ROOT / "tools/local/check_migrations.py")], check=True)
    elif args.action == "migrate":
        for name in ("broker", "configs"):
            subprocess.run(command + ["run", "--rm", f"migrate-{name}"], check=True)
    else:
        if args.action == 'up':
            # Старые процессы не должны менять справочники во время первого переноса.
            subprocess.run(command + ['stop', 'shipments', 'transportations'], check=True)
        subprocess.run(command + actions[args.action], check=True)


if __name__ == "__main__":
    main()

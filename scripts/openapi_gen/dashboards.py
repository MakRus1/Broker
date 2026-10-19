"""Воспроизводимые сервисные Grafana-дашборды по метрикам userver."""

import hashlib
import json
import re

SECTIONS = ('http', 'process', 'http_clients', 'postgresql', 'grpc', 'logs')
SELECTOR = 'service="$service",environment=~"$environment",instance=~"$instance"'
PUBLIC_HTTP = SELECTOR + ',http_path!~"/(ping(/ready)?|metrics|tests/.*)"'


def settings(spec):
    if not isinstance(spec, dict):
        raise ValueError('service.yaml: ожидается объект')
    value = spec.get('dashboard', {})
    if not isinstance(value, dict) or set(value) - {
        'title',
        'description',
        'tags',
        'refresh',
        'sections',
        'panels',
    }:
        raise ValueError(
            'service.yaml: dashboard поддерживает title/description/tags/refresh/sections/panels'
        )
    for key in ('title', 'description'):
        if key in value and (not isinstance(value[key], str) or not value[key].strip()):
            raise ValueError(f'service.yaml: dashboard.{key} — непустая строка')
    tags = value.get('tags', [])
    if not isinstance(tags, list) or any(
        not isinstance(tag, str) or not tag.strip() for tag in tags
    ):
        raise ValueError('service.yaml: dashboard.tags — список непустых строк')
    if value.get('refresh', '10s') not in ('5s', '10s', '30s', '1m', '5m'):
        raise ValueError('service.yaml: dashboard.refresh — 5s/10s/30s/1m/5m')
    defaults = ['http', 'process', 'http_clients']
    for section in ('postgresql', 'grpc'):
        if spec.get(section, {}).get('enabled', False):
            defaults.append(section)
    sections = value.get('sections', [*defaults, 'logs'])
    if (
        not isinstance(sections, list)
        or any(not isinstance(item, str) or item not in SECTIONS for item in sections)
        or len(sections) != len(set(sections))
    ):
        raise ValueError(f'service.yaml: dashboard.sections — уникальные имена из {SECTIONS}')
    for section in ('postgresql', 'grpc'):
        if section in sections and not spec.get(section, {}).get('enabled', False):
            raise ValueError(f'service.yaml: dashboard.sections требует {section}.enabled: true')
    panels = value.get('panels', [])
    if not isinstance(panels, list):
        raise ValueError('service.yaml: dashboard.panels — список PromQL-панелей')
    for panel in panels:
        if not isinstance(panel, dict) or set(panel) - {
            'title',
            'expr',
            'unit',
            'legend',
            'description',
        }:
            raise ValueError(
                'service.yaml: панель dashboard.panels поддерживает title/expr/unit/legend/description'
            )
        for field in ('title', 'expr'):
            if not isinstance(panel.get(field), str) or not panel[field].strip():
                raise ValueError(
                    f'service.yaml: dashboard.panels.{field} — обязательная непустая строка'
                )
        for field in ('unit', 'legend', 'description'):
            if field in panel and not isinstance(panel[field], str):
                raise ValueError(f'service.yaml: dashboard.panels.{field} — строка')
    return value, sections, panels


def query(metric, extra='', selector=SELECTOR):
    return metric + '{' + selector + (',' + extra if extra else '') + '}'


def rate(metric, extra='', selector=SELECTOR):
    return 'rate(' + query(metric, extra, selector) + '[$__rate_interval])'


def panel(title, expr, unit='short', legend='{{instance}}', description=''):
    return {
        'title': title,
        'expr': expr,
        'unit': unit,
        'legend': legend,
        'description': description,
    }


def standard(section):
    if section == 'http':
        return [
            panel(
                'HTTP: запросов / с',
                'sum by(http_path) (' + rate('http_handler_rps', selector=PUBLIC_HTTP) + ')',
                'reqps',
                '{{http_path}}',
            ),
            panel(
                'HTTP: 5xx / с',
                'sum by(http_path) ('
                + rate('http_handler_reply_codes', 'http_code=~"5.."', PUBLIC_HTTP)
                + ')',
                'reqps',
                '{{http_path}}',
            ),
            panel(
                'HTTP: p95, максимум по инстансам',
                'max by(http_path) ('
                + query('http_handler_timings', 'percentile="p95"', PUBLIC_HTTP)
                + ')',
                'ms',
                '{{http_path}}',
                'Готовый p95 userver. Максимум локальных p95, а не общий p95 распределения; служебные ручки исключены.',
            ),
            panel(
                'HTTP: доля 5xx',
                '100 * sum('
                + rate('http_handler_reply_codes', 'http_code=~"5.."', PUBLIC_HTTP)
                + ') / clamp_min(sum('
                + rate('http_handler_rps', selector=PUBLIC_HTTP)
                + '), 1e-9)',
                'percent',
            ),
        ]
    if section == 'process':
        return [
            panel(
                'Доступность /metrics',
                query('up'),
                'short',
                '{{instance}}',
                'Успех сбора Prometheus. Готовность принимать бизнес-запросы проверяется отдельно через /ping/ready.',
            ),
            panel('Время работы процесса', query('engine_uptime_seconds'), 's'),
            panel(
                'Нагрузка event threads',
                query('engine_ev_threads_cpu_load_percent'),
                'percent',
                '{{instance}} {{ev_thread_name}}',
            ),
            panel(
                'Живые задачи',
                query('engine_task_processors_tasks_alive'),
                'short',
                '{{instance}} {{task_processor}}',
            ),
            panel(
                'Задачи в очереди',
                query('engine_task_processors_tasks_queued'),
                'short',
                '{{instance}} {{task_processor}}',
            ),
        ]
    if section == 'http_clients':
        return [
            panel(
                'HTTP-клиенты: ответы / с',
                'sum by(http_code) (' + rate('httpclient_reply_statuses') + ')',
                'reqps',
                '{{http_code}}',
            ),
            panel(
                'HTTP-клиенты: ошибки / с',
                'sum by(http_error) (' + rate('httpclient_errors', 'http_error!="ok"') + ')',
                'reqps',
                '{{http_error}}',
            ),
            panel(
                'HTTP-клиенты: p95, максимум по инстансам',
                'max(' + query('httpclient_timings', 'percentile="p95"') + ')',
                'ms',
            ),
            panel('HTTP-клиенты: повторы / с', 'sum(' + rate('httpclient_retries') + ')', 'reqps'),
        ]
    if section == 'postgresql':
        return [
            panel(
                'PostgreSQL: соединения',
                query('postgresql_connections_active'),
                'short',
                '{{postgresql_database}} {{postgresql_instance}}',
            ),
            panel(
                'PostgreSQL: занятые соединения',
                query('postgresql_connections_busy'),
                'short',
                '{{postgresql_database}} {{postgresql_instance}}',
            ),
            panel(
                'PostgreSQL: ожидание соединения',
                query('postgresql_connections_waiting'),
                'short',
                '{{postgresql_database}} {{postgresql_instance}}',
            ),
            panel(
                'PostgreSQL: ошибки / с',
                'sum by(postgresql_database) (' + rate('postgresql_errors_v2') + ')',
                'reqps',
                '{{postgresql_database}}',
            ),
        ]
    if section == 'grpc':
        return [
            panel(
                'gRPC: запросов / с',
                'sum by(grpc_destination) (' + rate('grpc_server_by_destination_rps') + ')',
                'reqps',
                '{{grpc_destination}}',
            ),
            panel(
                'gRPC: ошибки / с',
                'sum by(grpc_destination) ('
                + rate('grpc_server_by_destination_status', 'grpc_code!="OK"')
                + ')',
                'reqps',
                '{{grpc_destination}}',
            ),
            panel(
                'gRPC: p95, максимум по инстансам',
                'max by(grpc_destination) ('
                + query('grpc_server_by_destination_timings', 'percentile="p95"')
                + ')',
                'ms',
                '{{grpc_destination}}',
            ),
            panel(
                'gRPC: активные запросы',
                query('grpc_server_by_destination_active'),
                'short',
                '{{grpc_destination}} {{instance}}',
            ),
        ]
    return []


def variables(service, logs):
    def variable(name, expression, label):
        return {
            'name': name,
            'label': label,
            'type': 'query',
            'refresh': 1,
            'datasource': {'type': 'prometheus', 'uid': '${prometheus}'},
            'query': {'query': expression, 'refId': name},
            'definition': expression,
            'multi': True,
            'includeAll': True,
            'allValue': '.*',
            'current': {'text': 'All', 'value': '$__all'},
            'sort': 1,
        }

    result = [
        {
            'name': 'service',
            'type': 'constant',
            'hide': 2,
            'query': service,
            'current': {'text': service, 'value': service},
        },
        {
            'name': 'prometheus',
            'label': 'Метрики',
            'type': 'datasource',
            'query': 'prometheus',
            'current': {'text': 'Prometheus', 'value': 'prometheus'},
        },
        variable(
            'environment',
            'label_values(engine_uptime_seconds{service="$service"}, environment)',
            'Окружение',
        ),
        variable(
            'instance',
            'label_values(engine_uptime_seconds{service="$service",environment=~"$environment"}, instance)',
            'Инстанс (метрики)',
        ),
    ]
    if logs:
        result.insert(
            2,
            {
                'name': 'loki',
                'label': 'Логи',
                'type': 'datasource',
                'query': 'loki',
                'current': {'text': 'Loki', 'value': 'loki'},
            },
        )
    return result


def render(service, spec):
    if not re.fullmatch(r'[a-z][a-z0-9_]*', service):
        raise ValueError('Некорректное имя сервиса для dashboard')
    options, sections, custom = settings(spec)
    uid = 'broker-service-' + service
    if len(uid) > 40:
        uid = uid[:29] + '-' + hashlib.sha256(service.encode()).hexdigest()[:10]
    dashboard = {
        'uid': uid,
        'title': options.get('title', f'Broker · {service}'),
        'description': options.get(
            'description',
            'Сгенерировано из service.yaml; изменения в JSON будут заменены make gen.',
        ),
        'tags': list(dict.fromkeys(['broker', 'service', service, *options.get('tags', [])])),
        'schemaVersion': 39,
        'version': 1,
        'editable': False,
        'timezone': 'browser',
        'refresh': options.get('refresh', '10s'),
        'time': {'from': 'now-1h', 'to': 'now'},
        'templating': {'list': variables(service, 'logs' in sections)},
        'panels': [],
    }
    titles = {
        'http': 'HTTP-сервер',
        'process': 'Процесс',
        'http_clients': 'HTTP-клиенты',
        'postgresql': 'PostgreSQL',
        'grpc': 'gRPC-сервер',
        'logs': 'Логи',
    }
    y = 0
    for section in [*sections, *(['custom'] if custom else [])]:
        base = 1000 if section == 'custom' else (SECTIONS.index(section) + 1) * 100
        dashboard['panels'].append(
            {
                'id': base,
                'type': 'row',
                'title': titles.get(section, 'Метрики сервиса'),
                'collapsed': False,
                'panels': [],
                'gridPos': {'x': 0, 'y': y, 'w': 24, 'h': 1},
            }
        )
        y += 1
        if section == 'logs':
            dashboard['panels'].append(
                {
                    'id': base + 1,
                    'title': 'Логи сервиса · trace_id → Tempo',
                    'type': 'logs',
                    'description': 'Окружение фильтруется как в метриках. Инстанс применяется только к метрикам: у логов пока нет соответствующего label.',
                    'datasource': {'type': 'loki', 'uid': '${loki}'},
                    'gridPos': {'x': 0, 'y': y, 'w': 24, 'h': 10},
                    'targets': [
                        {
                            'refId': 'A',
                            'expr': '{service_name="$service",deployment_environment_name=~"$environment"}',
                        }
                    ],
                    'options': {
                        'showTime': True,
                        'wrapLogMessage': True,
                        'sortOrder': 'Descending',
                    },
                }
            )
            y += 10
            continue
        entries = custom if section == 'custom' else standard(section)
        for index, entry in enumerate(entries):
            dashboard['panels'].append(
                {
                    'id': base + index + 1,
                    'title': entry['title'],
                    'type': 'timeseries',
                    'description': entry.get('description', ''),
                    'datasource': {'type': 'prometheus', 'uid': '${prometheus}'},
                    'gridPos': {'x': index % 2 * 12, 'y': y + index // 2 * 8, 'w': 12, 'h': 8},
                    'targets': [
                        {
                            'refId': 'A',
                            'expr': entry['expr'],
                            'legendFormat': entry.get('legend', '{{instance}}'),
                        }
                    ],
                    'fieldConfig': {
                        'defaults': {'unit': entry.get('unit', 'short')},
                        'overrides': [],
                    },
                    'options': {
                        'legend': {'displayMode': 'table', 'placement': 'bottom'},
                        'tooltip': {'mode': 'multi'},
                    },
                }
            )
        y += (len(entries) + 1) // 2 * 8
    return json.dumps(dashboard, ensure_ascii=False, indent=2) + '\n'


def provision(root, destination, loader):
    """Подготовить каталог provisioning, не меняя исходники сервисов."""
    import yaml

    planned = {'local.json': (root / 'tools/local/grafana/dashboard.json').read_text()}
    for manifest in sorted((root / 'services').glob('*/service.yaml')):
        service = manifest.parent
        expected = render(service.name, yaml.load(manifest.read_text(), Loader=loader))
        path = service / 'monitoring/grafana.json'
        if not path.exists() or path.read_text() != expected:
            raise ValueError(
                f'{path}: dashboard устарел; выполните make gen SERVICE={service.name}'
            )
        planned[f'service-{service.name}.json'] = expected
    destination.mkdir(parents=True, exist_ok=True)
    for name, content in planned.items():
        path = destination / name
        if not path.exists() or path.read_text() != content:
            path.write_text(content)
    # Удаляем только сгенерированные копии удаленных сервисов в каталоге стенда.
    for path in destination.glob('service-*.json'):
        if path.name not in planned:
            path.unlink()

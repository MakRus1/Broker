"""Сбор Kubernetes/CNPG-ресурсов и разделы в сервисных дашбордах staging."""

import json
from copy import deepcopy

from platform_resources import load


SA_TOKEN = '/var/run/secrets/kubernetes.io/serviceaccount/'


def labels(owners, environment):
    rules = []
    for owner in owners:
        name = owner.replace('_', '-')
        rules += [
            {
                'source_labels': ['pod'],
                'regex': name + '-[a-z0-9]+-[a-z0-9]+',
                'target_label': 'broker_service',
                'replacement': owner,
            },
            {
                'source_labels': ['pod'],
                'regex': name + '-[a-z0-9]+-[a-z0-9]+',
                'target_label': 'broker_workload',
                'replacement': 'application',
            },
        ]
        for source in ('pod', 'persistentvolumeclaim'):
            rules += [
                {
                    'source_labels': [source],
                    'regex': name + '-postgres-[0-9]+',
                    'target_label': 'broker_service',
                    'replacement': owner,
                },
                {
                    'source_labels': [source],
                    'regex': name + '-postgres-[0-9]+',
                    'target_label': 'broker_workload',
                    'replacement': 'postgres',
                },
            ]
    rules += [
        {'source_labels': ['broker_service'], 'regex': '.+', 'action': 'keep'},
        {'target_label': 'environment', 'replacement': environment},
    ]
    return rules


def scrape_configs(namespace, owners, environment):
    resources = labels(owners, environment)
    jobs = []
    for suffix, path in (('cadvisor', '/metrics/cadvisor'), ('volumes', '/metrics')):
        jobs.append(
            {
                'job_name': 'kubernetes-' + suffix,
                'scheme': 'https',
                'authorization': {'credentials_file': SA_TOKEN + 'token'},
                'tls_config': {'ca_file': SA_TOKEN + 'ca.crt'},
                'kubernetes_sd_configs': [{'role': 'node'}],
                'relabel_configs': [
                    {'source_labels': ['__meta_kubernetes_node_name'], 'target_label': 'node'},
                    {
                        'target_label': '__address__',
                        'replacement': 'kubernetes.default.svc:443',
                    },
                    {
                        'source_labels': ['__meta_kubernetes_node_name'],
                        'target_label': '__metrics_path__',
                        'replacement': '/api/v1/nodes/$1/proxy' + path,
                    },
                ],
                'metric_relabel_configs': [
                    {'source_labels': ['namespace'], 'regex': namespace, 'action': 'keep'},
                    *resources,
                ],
            }
        )
    jobs.append(
        {
            'job_name': 'kube-state-metrics',
            'static_configs': [{'targets': ['kube-state-metrics:8080']}],
            'metric_relabel_configs': [
                {'source_labels': ['namespace'], 'regex': namespace, 'action': 'keep'},
                *resources,
            ],
        }
    )
    jobs.append(
        {
            'job_name': 'postgresql-instances',
            'kubernetes_sd_configs': [{'role': 'pod', 'namespaces': {'names': [namespace]}}],
            'relabel_configs': [
                {
                    'source_labels': ['__meta_kubernetes_pod_label_cnpg_io_cluster'],
                    'regex': '.+-postgres',
                    'action': 'keep',
                },
                {
                    'source_labels': ['__meta_kubernetes_pod_container_port_number'],
                    'regex': '9187',
                    'action': 'keep',
                },
                {
                    'source_labels': ['__meta_kubernetes_pod_phase'],
                    'regex': 'Running',
                    'action': 'keep',
                },
                {'source_labels': ['__meta_kubernetes_pod_name'], 'target_label': 'pod'},
            ],
            'metric_relabel_configs': resources,
        }
    )
    return [deepcopy(job) for job in jobs]


def objects(root, namespace, environment):
    from staging_manifest import document, service, port, workload

    name = 'kube-state-metrics'
    profile = load(root / f'tools/deploy/{name}/platform.yaml', environment)['application']
    if profile['replicas'] != 1:
        raise ValueError('kube-state-metrics: используйте одну реплику без дублирования метрик')
    image = json.loads((root / f'tools/deploy/{name}/image.json').read_text())['image']
    result = [
        document('ServiceAccount', name, namespace),
        document('ServiceAccount', 'prometheus', namespace),
    ]
    role = document(
        'Role',
        'resource-monitoring',
        namespace,
        rules=[
            {
                'apiGroups': [''],
                'resources': ['pods', 'persistentvolumeclaims'],
                'verbs': ['get', 'list', 'watch'],
            },
            {
                'apiGroups': ['apps'],
                'resources': ['deployments', 'statefulsets'],
                'verbs': ['get', 'list', 'watch'],
            },
        ],
    )
    role['apiVersion'] = 'rbac.authorization.k8s.io/v1'
    binding = document(
        'RoleBinding',
        'resource-monitoring',
        namespace,
        roleRef={
            'apiGroup': 'rbac.authorization.k8s.io',
            'kind': 'Role',
            'name': 'resource-monitoring',
        },
        subjects=[
            {'kind': 'ServiceAccount', 'name': owner, 'namespace': namespace}
            for owner in (name, 'prometheus')
        ],
    )
    binding['apiVersion'] = 'rbac.authorization.k8s.io/v1'
    cluster_role = {
        'apiVersion': 'rbac.authorization.k8s.io/v1',
        'kind': 'ClusterRole',
        'metadata': {'name': namespace + '-kubelet-metrics'},
        'rules': [
            {'apiGroups': [''], 'resources': ['nodes'], 'verbs': ['get', 'list', 'watch']},
            {'apiGroups': [''], 'resources': ['nodes/proxy'], 'verbs': ['get']},
        ],
    }
    cluster_binding = {
        'apiVersion': 'rbac.authorization.k8s.io/v1',
        'kind': 'ClusterRoleBinding',
        'metadata': {'name': namespace + '-kubelet-metrics'},
        'roleRef': {
            'apiGroup': 'rbac.authorization.k8s.io',
            'kind': 'ClusterRole',
            'name': cluster_role['metadata']['name'],
        },
        'subjects': [{'kind': 'ServiceAccount', 'name': 'prometheus', 'namespace': namespace}],
    }
    result += [role, binding, cluster_role, cluster_binding]
    container = {
        'name': name,
        'image': image,
        'args': [
            '--namespaces=' + namespace,
            '--resources=pods,deployments,statefulsets,persistentvolumeclaims',
        ],
        'ports': [{'containerPort': 8080}],
        'resources': profile['resources'],
        'securityContext': {
            'runAsNonRoot': True,
            'runAsUser': 65534,
            'allowPrivilegeEscalation': False,
            'readOnlyRootFilesystem': True,
            'capabilities': {'drop': ['ALL']},
        },
        'readinessProbe': {'httpGet': {'path': '/healthz', 'port': 8080}},
    }
    deployment = workload('Deployment', name, namespace, container)
    deployment['spec']['template']['spec'].update(
        serviceAccountName=name, automountServiceAccountToken=True
    )
    return [*result, deployment, service(name, namespace, [port('metrics', 8080)])]


def dashboard(text, *, postgres):
    value = json.loads(text)
    y = max(
        (panel['gridPos']['y'] + panel['gridPos']['h'] for panel in value['panels']), default=0
    )
    value['panels'].append(
        {
            'id': 800,
            'type': 'row',
            'title': 'Ресурсы Kubernetes',
            'collapsed': False,
            'gridPos': {'x': 0, 'y': y, 'w': 24, 'h': 1},
        }
    )
    y += 1
    expression = 'label_values(kube_pod_container_resource_requests{broker_service="$service",environment=~"$environment"}, pod)'
    value['templating']['list'].append(
        {
            'name': 'pod',
            'label': 'Pod (ресурсы)',
            'type': 'query',
            'refresh': 1,
            'datasource': {'type': 'prometheus', 'uid': '${prometheus}'},
            'query': {'query': expression, 'refId': 'pod'},
            'definition': expression,
            'multi': True,
            'includeAll': True,
            'allValue': '.*',
            'current': {'text': 'All', 'value': '$__all'},
            'sort': 1,
        }
    )
    selector = 'broker_service="$service",environment=~"$environment",pod=~"$pod"'
    entries = []
    for kind, title in [
        ('application', 'Приложение'),
        *([('postgres', 'PostgreSQL')] if postgres else []),
    ]:
        selected = selector + ',broker_workload="' + kind + '"'
        for resource, metric, unit in [
            ('cpu', 'rate(container_cpu_usage_seconds_total', 'cores'),
            ('memory', 'container_memory_working_set_bytes', 'bytes'),
        ]:
            usage = metric + '{' + selected + ',container!="",container!="POD"}'
            usage = usage + '[$__rate_interval])' if resource == 'cpu' else usage
            targets = [(f'sum by(pod) ({usage})', '{{pod}} · потребление')]
            for source, label in [('requests', 'request'), ('limits', 'limit')]:
                targets.append(
                    (
                        'sum by(pod) (kube_pod_container_resource_'
                        + source
                        + '{'
                        + selected
                        + ',resource="'
                        + resource
                        + '"})',
                        '{{pod}} · ' + label,
                    )
                )
            entries.append(
                (
                    title + ': ' + ('CPU' if resource == 'cpu' else 'память'),
                    targets,
                    unit,
                    'На каждый pod. CPU в ядрах; отсутствие линии limit означает отсутствие лимита.',
                )
            )
        entries.append(
            (
                title + ': CPU throttling',
                [
                    (
                        '100 * sum by(pod) (rate(container_cpu_cfs_throttled_periods_total{'
                        + selected
                        + '}[$__rate_interval])) / clamp_min(sum by(pod) (rate(container_cpu_cfs_periods_total{'
                        + selected
                        + '}[$__rate_interval])), 1e-9)',
                        '{{pod}}',
                    )
                ],
                'percent',
                'Доля периодов, в которых CPU ограничивался квотой.',
            )
        )
        entries.append(
            (
                title + ': перезапуски за час',
                [
                    (
                        'sum by(pod) (increase(kube_pod_container_status_restarts_total{'
                        + selected
                        + '}[1h]))',
                        '{{pod}}',
                    )
                ],
                'short',
                'Включает перезапуски после OOM и других сбоев.',
            )
        )
    if postgres:
        entries += [
            (
                'PostgreSQL: доступные экземпляры',
                [('sum(max by(pod) (cnpg_collector_up{' + selector + '}))', 'Доступно')],
                'short',
                '',
            ),
            (
                'PostgreSQL: primary / replica',
                [('cnpg_pg_replication_in_recovery{' + selector + '}', '{{pod}}')],
                'short',
                '0 — primary, 1 — replica.',
            ),
            (
                'PostgreSQL: задержка репликации',
                [('cnpg_pg_replication_lag{' + selector + '}', '{{pod}}')],
                's',
                '',
            ),
            (
                'PostgreSQL: размер базы',
                [('cnpg_pg_database_size_bytes{' + selector + '}', '{{pod}} {{datname}}')],
                'bytes',
                'Размер базы; не включает все расходы PVC и WAL.',
            ),
        ]
    for index, (title, targets, unit, description) in enumerate(entries):
        value['panels'].append(
            {
                'id': 801 + index,
                'type': 'timeseries',
                'title': title,
                'description': description,
                'datasource': {'type': 'prometheus', 'uid': '${prometheus}'},
                'gridPos': {'x': index % 2 * 12, 'y': y + index // 2 * 8, 'w': 12, 'h': 8},
                'targets': [
                    {'refId': chr(65 + n), 'expr': expr, 'legendFormat': legend}
                    for n, (expr, legend) in enumerate(targets)
                ],
                'fieldConfig': {'defaults': {'unit': unit, 'min': 0}, 'overrides': []},
                'options': {'legend': {'displayMode': 'table', 'placement': 'bottom'}},
            }
        )
    return json.dumps(value, ensure_ascii=False, indent=2) + '\n'

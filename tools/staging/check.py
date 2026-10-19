#!/usr/bin/env python3
"""Проверка реальных метрик, логов и трассы в staging без паролей пользователей."""

import argparse
import json
import subprocess
import time
import sys
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from service_catalog import discover


def probe_trace(url, *, timeout=60):
    deadline = time.monotonic() + timeout
    while True:
        try:
            with urlopen(url, timeout=5) as response:
                trace = response.headers.get('X-Trace-Id')
                response.read()
            if not trace or len(trace) != 32:
                raise ValueError('Сервис не вернул X-Trace-Id')
            return trace
        except HTTPError as exc:
            # EndpointSlice и соединения могут переключиться позже завершения rollout.
            if exc.code not in (502, 503, 504):
                raise
            exc.close()
            error = exc
        except (URLError, TimeoutError, ConnectionError) as exc:
            error = exc
        if time.monotonic() >= deadline:
            raise ValueError(
                f'Сервис не готов к проверке трассы за {timeout} секунд: {error}'
            ) from error
        print(f'Ожидаю доступность сервиса после rollout: {error}', flush=True)
        time.sleep(min(2, max(0, deadline - time.monotonic())))


def check(context, namespace):
    result = subprocess.check_output(
        ['kubectl', '--context', context, '-n', namespace, 'get', 'services', '-o', 'json'],
        text=True,
    )
    ips = {
        item['metadata']['name']: item['spec']['clusterIP'] for item in json.loads(result)['items']
    }

    def fetch(name, port, path):
        request = Request(
            f'http://{ips[name]}:{port}{path}', headers={'Accept': 'application/json'}
        )
        with urlopen(request, timeout=5) as response:
            return json.load(response)

    deployments = json.loads(
        subprocess.check_output(
            ['kubectl', '--context', context, '-n', namespace, 'get', 'deployments', '-o', 'json'],
            text=True,
        )
    )
    installed = {item['metadata']['name'] for item in deployments['items']}
    expected = {name for name in discover() if name.replace('_', '-') in installed}
    if not expected:
        raise ValueError('В кластере нет сервисов из каталога')
    deadline = time.monotonic() + 60
    while True:
        metrics = fetch(
            'prometheus', 9090, '/api/v1/query?' + urlencode({'query': 'up{job="services"}'})
        )
        series = metrics['data']['result']
        up = {item['metric'].get('service'): float(item['value'][1]) for item in series}
        if all(up.get(name) == 1 for name in expected):
            break
        if time.monotonic() > deadline:
            raise ValueError('Не все установленные сервисы доступны Prometheus')
        time.sleep(2)
    trace = probe_trace(f'http://{ips["broker"]}:8080/hello?name=staging-monitoring-check')
    deadline = time.monotonic() + 60
    while True:
        try:
            fetch('tempo', 3200, '/api/traces/' + trace)
            query = '{service_name="broker",level=~".+"} |= "' + trace + '"'
            logs = fetch(
                'loki',
                3100,
                '/loki/api/v1/query_range?'
                + urlencode(
                    {
                        'query': query,
                        'start': time.time_ns() - 300_000_000_000,
                        'end': time.time_ns(),
                        'limit': 10,
                    }
                ),
            )
            if logs['data']['result']:
                break
        except HTTPError as exc:
            if exc.code != 404:
                raise
        if time.monotonic() > deadline:
            raise ValueError(f'Нет связанной трассы и логов для {trace}')
        time.sleep(2)
    if fetch('grafana', 3000, '/api/health')['database'] != 'ok':
        raise ValueError('Grafana не готова')
    print(
        f'OK: все установленные сервисы в Prometheus; trace_id={trace} найден в Tempo и Loki с label level; Grafana готова.'
    )


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--context', required=True)
    parser.add_argument('--namespace', default='broker-staging')
    args = parser.parse_args()
    check(args.context, args.namespace)

#!/usr/bin/env python3
"""Изолированный smoke двух реплик каждого сервиса с настоящим PostgreSQL и тестовым OIDC."""

import argparse
import contextlib
import http.server
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

import psycopg2
import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/support'))
from auth_tokens import SigningKey

ORGANIZATION = '11111111-1111-4111-8111-111111111111'
CUSTOMER = '33333333-3333-4333-8333-333333333333'
USER_PERMISSIONS = [
    'counterparties.read',
    'organizations.read_employees',
    'shipments.read',
    'shipments.write',
    'transportations.read',
]


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def post(port, path, body, token, expected=200):
    request = urllib.request.Request(
        f'http://127.0.0.1:{port}{path}',
        data=json.dumps(body).encode(),
        headers={'Content-Type': 'application/json', 'Authorization': 'Bearer ' + token},
    )
    try:
        response = urllib.request.urlopen(request, timeout=5)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        data = response.read()
        assert response.status == expected, (response.status, data.decode())
        return json.loads(data)


def run(build):
    with tempfile.TemporaryDirectory(
        prefix='broker-logistics-smoke-'
    ) as directory, contextlib.ExitStack() as stack:
        work = Path(directory)
        key = SigningKey(work / 'key.pem', 'smoke-key')
        pgport = free_port()
        pg_bin = Path(os.environ.get('POSTGRES_BIN', '/usr/lib/postgresql/16/bin'))
        subprocess.run(
            [str(pg_bin / 'initdb'), '-D', str(work / 'pg'), '-A', 'trust', '-U', 'smoke'],
            check=True,
            capture_output=True,
        )
        subprocess.run(
            [
                str(pg_bin / 'pg_ctl'),
                '-D',
                str(work / 'pg'),
                '-l',
                str(work / 'postgres.log'),
                '-o',
                f'-p {pgport} -h 127.0.0.1 -k {directory}',
                '-w',
                'start',
            ],
            check=True,
            capture_output=True,
        )
        stack.callback(
            lambda: subprocess.run(
                [str(pg_bin / 'pg_ctl'), '-D', str(work / 'pg'), '-m', 'immediate', 'stop'],
                capture_output=True,
            )
        )

        class Oidc(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_GET(self):
                self.respond({'keys': [key.jwk]})

            def do_POST(self):
                self.rfile.read(int(self.headers.get('Content-Length', '0')))
                if self.path == '/access':
                    return self.respond(
                        {
                            'organization_id': ORGANIZATION,
                            'permissions': USER_PERMISSIONS,
                        }
                    )
                self.respond({'access_token': make_token('dispatcher', ['transportations.create'])})

            def respond(self, value):
                data = json.dumps(value).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        oidc = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Oidc)
        stack.callback(oidc.server_close)
        threading.Thread(target=oidc.serve_forever, daemon=True).start()
        stack.callback(oidc.shutdown)
        issuer = f'http://127.0.0.1:{oidc.server_port}'

        def make_token(subject, roles):
            return key.token(
                subject,
                roles=roles,
                claims={
                    'iss': issuer,
                    'aud': 'broker-logistics',
                    'azp': 'shipments-dispatcher' if subject == 'dispatcher' else 'broker-web',
                    'resource_access': {'broker-logistics': {'roles': roles}},
                },
            )

        admin = psycopg2.connect(host='127.0.0.1', port=pgport, user='smoke', dbname='postgres')
        admin.autocommit = True
        stack.callback(admin.close)
        for service in ['shipments', 'transportations']:
            with admin.cursor() as cursor:
                cursor.execute('CREATE DATABASE ' + service)
            db = psycopg2.connect(host='127.0.0.1', port=pgport, user='smoke', dbname=service)
            stack.callback(db.close)
            with db.cursor() as cursor:
                migrations = sorted(
                    (ROOT / f'services/{service}/postgresql/schemas/db_1').glob('V*.sql')
                )
                for migration in migrations:
                    cursor.execute(migration.read_text())
            db.commit()

        def start(service, index, receiver=None):
            source = ROOT / 'services' / service
            config = yaml.safe_load((source / 'configs/static_config.yaml').read_text())
            variables = yaml.safe_load((source / 'configs/config_vars.yaml').read_text())
            port, monitor = free_port(), free_port()
            variables.update(
                {
                    'server-port': port,
                    'monitor-port': monitor,
                    'pg-connection': f'postgresql://smoke@127.0.0.1:{pgport}/{service}',
                    'auth-issuer': issuer,
                    'auth-jwks-url': issuer + '/jwks',
                    'auth-allow-http': True,
                    'organization-access-url': issuer + '/access',
                    'logger-level': 'warning',
                }
            )
            if receiver:
                variables.update(
                    {
                        'dispatcher-enabled': True,
                        'dispatcher-token-url': issuer + '/token',
                        'dispatcher-client-id': 'smoke-dispatcher',
                        'dispatcher-client-secret': 'smoke-only',
                        'client-transportations-base-url': f'http://127.0.0.1:{receiver}',
                    }
                )
            config_path = work / f'{service}-{index}.yaml'
            vars_path = work / f'{service}-{index}-vars.yaml'
            config_path.write_text(yaml.safe_dump(config))
            vars_path.write_text(yaml.safe_dump(variables))
            log = stack.enter_context((work / f'{service}-{index}.log').open('w+'))
            process = subprocess.Popen(
                [
                    str(build / f'services/{service}/{service}'),
                    '--config',
                    str(config_path),
                    '--config_vars',
                    str(vars_path),
                ],
                stdout=log,
                stderr=log,
            )

            def stop():
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()

            stack.callback(stop)
            for _ in range(100):
                if process.poll() is not None:
                    log.seek(0)
                    raise RuntimeError(log.read())
                try:
                    with urllib.request.urlopen(
                        f'http://127.0.0.1:{monitor}/ping/ready', timeout=0.2
                    ) as response:
                        if response.status == 200:
                            return port
                except (OSError, urllib.error.HTTPError):
                    time.sleep(0.1)
            raise RuntimeError(f'{service} не готов')

        receivers = [start('transportations', index) for index in range(2)]
        senders = [start('shipments', index, receivers[index]) for index in range(2)]
        token = make_token('author', USER_PERMISSIONS)
        shipment = post(
            senders[0],
            '/api/v1/shipments/create',
            {'customer_id': CUSTOMER, 'logist': None, 'idempotency_key': 'create-shipment'},
            token,
            201,
        )['shipment']
        item = post(
            senders[1],
            '/api/v1/shipment-items/add',
            {
                'shipment_id': shipment['id'],
                'expected_revision': 1,
                'container_type_code': '40HC',
                'container_ownership': 'COC',
                'planned_quantity': 5,
                'idempotency_key': 'add-item',
            },
            token,
            201,
        )['item']
        operations = []
        for index, quantity in enumerate([3, 2]):
            operations.append(
                post(
                    senders[index],
                    '/api/v1/transportation-operations/create',
                    {
                        'item_id': item['id'],
                        'expected_revision': 1,
                        'quantity': quantity,
                        'idempotency_key': 'operation-' + str(index),
                    },
                    token,
                    202,
                )['operation']
            )
        for operation in operations:
            for attempt in range(100):
                state = post(
                    senders[attempt % 2],
                    '/api/v1/transportation-operations/get',
                    {'operation_id': operation['id']},
                    token,
                )['operation']
                if state['status'] == 'succeeded':
                    assert len(state['transportation_ids']) == operation['quantity']
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError('Не завершилась доставка операции')
        for receiver in receivers:
            result = post(
                receiver, '/api/v1/transportations/list', {'shipment_id': shipment['id']}, token
            )
            assert len(result['items']) == 5
            assert len({value['id'] for value in result['items']}) == 5
        counter = post(
            senders[0], '/api/v1/shipment-items/list', {'shipment_id': shipment['id']}, token
        )['items'][0]
        assert (
            counter['created_quantity'],
            counter['pending_quantity'],
            counter['available_quantity'],
        ) == (5, 0, 0)
        print('OK: 2 shipments + 2 transportations, 5 перевозок, общий inbox/outbox без дублей')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    run(parser.parse_args().build_dir.resolve())

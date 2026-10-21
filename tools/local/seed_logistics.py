"""Только локальный стенд: тестовые сотрудники привязаны к одному демонстрационному tenant."""

import json
import os
from pathlib import Path
import subprocess
import uuid

users = json.loads(Path('/cache/logistics-users.json').read_text())
for name in ('shipments', 'transportations', 'counterparties'):
    for subject in users:
        uuid.UUID(subject)
        subprocess.run(
            [
                'psql',
                '-h',
                'postgres-' + name,
                '-U',
                'testsuite',
                '-d',
                name + '_db_1',
                '-v',
                'ON_ERROR_STOP=1',
                '-v',
                'subject=' + subject,
            ],
            env={**os.environ, 'PGPASSWORD': 'testsuite'},
            text=True,
            check=True,
            input="""INSERT INTO tenant_memberships (issuer, subject, tenant_id)
                 VALUES ('http://localhost:18888/realms/broker', :'subject',
                         '11111111-1111-4111-8111-111111111111') ON CONFLICT DO NOTHING;""",
        )
print('Доступ локальных сотрудников к демонстрационному tenant готов')

# Демонстрационный клиент для ранее созданных локальных поставок.
subprocess.run(
    [
        'psql',
        '-h',
        'postgres-counterparties',
        '-U',
        'testsuite',
        '-d',
        'counterparties_db_1',
        '-v',
        'ON_ERROR_STOP=1',
    ],
    env={**os.environ, 'PGPASSWORD': 'testsuite'},
    text=True,
    check=True,
    input="""INSERT INTO counterparties (tenant_id, id, name)
         VALUES ('11111111-1111-4111-8111-111111111111',
                 '33333333-3333-4333-8333-333333333333', 'Демонстрационный клиент')
         ON CONFLICT DO NOTHING;""",
)

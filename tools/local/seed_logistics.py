"""Только локальный стенд: тестовые сотрудники привязаны к одному демонстрационному organization."""

import json
import os
from pathlib import Path
import subprocess
import uuid

default_roles = (Path('/workspaces/broker/services/organizations/postgresql/queries/create_default_roles.sql')
                 .read_text().replace('$1::UUID', "'11111111-1111-4111-8111-111111111111'::UUID").rstrip().removesuffix(';') + ' ON CONFLICT DO NOTHING;')

users = json.loads(Path('/cache/logistics-users.json').read_text())
for index, subject in enumerate(users[:2]):
    uuid.UUID(subject)
    username = ('author', 'reviewer')[index]
    role = ('administrator', 'viewer')[index]
    subprocess.run(
        ['psql', '-h', 'postgres-organizations', '-U', 'testsuite', '-d', 'organizations_db_1',
         '-v', 'ON_ERROR_STOP=1', '-v', 'subject=' + subject,
         '-v', 'username=' + username, '-v', 'role=' + role],
        env={**os.environ, 'PGPASSWORD': 'testsuite'}, text=True, check=True,
        input="""INSERT INTO organizations (id, name)
            VALUES ('11111111-1111-4111-8111-111111111111', 'Демонстрационная организация')
            ON CONFLICT DO NOTHING;
        """ + default_roles + """
        INSERT INTO memberships
            (organization_id, issuer, subject, username, display_name, email, role)
        VALUES ('11111111-1111-4111-8111-111111111111',
            'http://localhost:18888/realms/broker', :'subject', :'username', :'username',
            :'username' || '@localhost.test', :'role') ON CONFLICT DO NOTHING;""",
    )
print('Локальные сотрудники привязаны к демонстрационной организации')

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
    input="""INSERT INTO counterparties (organization_id, id, name)
         VALUES ('11111111-1111-4111-8111-111111111111',
                 '33333333-3333-4333-8333-333333333333', 'Демонстрационный клиент')
         ON CONFLICT DO NOTHING;""",
)

# Локальный стенд использует единственную рабочую версию каждого модуля.
import sys
sys.path.insert(0, '/workspaces/broker/scripts')
from permission_catalog import load, register_sql, reconcile_sql
catalog = load('/workspaces/broker')
import hashlib
version = hashlib.sha256(json.dumps(catalog, sort_keys=True).encode()).hexdigest()
for owner in sorted(path.name for path in Path('/workspaces/broker/services').iterdir() if (path / 'service.yaml').exists()):
    subprocess.run(
        ['psql', '-h', 'postgres-organizations', '-U', 'testsuite', '-d', 'organizations_db_1', '-v', 'ON_ERROR_STOP=1'],
        env={**os.environ, 'PGPASSWORD': 'testsuite'}, input=register_sql(owner, version, catalog) + reconcile_sql(owner, [version]), text=True, check=True)

# Изолированные технические аккаунты нужны локальным сценариям подготовки конфигов.
for client, subject in json.loads(Path('/cache/platform-users.json').read_text()).items():
    permissions = ['configs.read', 'configs.write' if client == 'local-author' else 'configs.approve']
    subprocess.run(
        ['psql', '-h', 'postgres-organizations', '-U', 'testsuite', '-d', 'organizations_db_1',
         '-v', 'ON_ERROR_STOP=1', '-v', 'subject=' + subject],
        env={**os.environ, 'PGPASSWORD': 'testsuite'}, text=True, check=True,
        input="INSERT INTO platform_access(issuer,subject,permissions,revision) VALUES "
              "('http://localhost:18888/realms/broker', :'subject', ARRAY[" + ','.join("'" + code + "'" for code in permissions) + "], 1) ON CONFLICT DO NOTHING;")

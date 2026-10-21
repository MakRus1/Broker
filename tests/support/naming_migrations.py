"""Проверка переноса старых данных в отдельной схеме с откатом после теста."""

import json

ORGANIZATION = '11111111-1111-4111-8111-111111111111'
ENTITY = '22222222-2222-4222-8222-222222222222'
ITEM = '33333333-3333-4333-8333-333333333333'
OPERATION = '44444444-4444-4444-8444-444444444444'


def check_naming_migration(db, service_source_dir):
    directory = service_source_dir / 'postgresql/schemas/db_1'
    migration = next(directory.glob('*__clarify_organization_and_route_names.sql'))
    service = service_source_dir.name
    count = {'counterparties': 3, 'shipments': 6, 'transportations': 5}[service]
    with db.cursor() as cursor:
        cursor.execute('BEGIN')
        try:
            cursor.execute('CREATE SCHEMA naming_migration')
            cursor.execute('SET LOCAL search_path = naming_migration, public')
            for path in sorted(directory.glob('V*.sql'))[:count]:
                cursor.execute(path.read_text().replace('public.', 'naming_migration.'))
            cursor.execute(
                'INSERT INTO tenant_memberships VALUES (%s,%s,%s)',
                ('issuer', 'subject', ORGANIZATION),
            )
            document = {
                'request': {
                    'leg_id': ITEM,
                    'legs': [{'id': ITEM}],
                    'tenant_id': ORGANIZATION,
                    'reason': 'Строка с ключом "legs": [] остаётся текстом',
                },
                'result': {'legs': [{'id': ITEM}], 'revision': 2},
            }
            cursor.execute(
                'INSERT INTO business_audit (tenant_id,actor,action,entity_id,document) '
                'VALUES (%s,%s,%s,%s,%s)',
                (ORGANIZATION, 'author', 'set_route', ENTITY, json.dumps(document)),
            )
            if service == 'transportations':
                cursor.execute(
                    'INSERT INTO processed_commands VALUES (%s,%s,%s,%s)',
                    (
                        ORGANIZATION,
                        OPERATION,
                        json.dumps({'tenant_id': ORGANIZATION}),
                        json.dumps({'legs': [{'id': ITEM}]}),
                    ),
                )
            if service == 'shipments':
                cursor.execute(
                    'INSERT INTO shipments (tenant_id,id,customer_id) VALUES (%s,%s,%s)',
                    (ORGANIZATION, ENTITY, ITEM),
                )
                cursor.execute(
                    'INSERT INTO shipment_items '
                    '(tenant_id,id,shipment_id,container_type_code,container_ownership,planned_quantity) '
                    'VALUES (%s,%s,%s,%s,%s,%s)',
                    (ORGANIZATION, ITEM, ENTITY, '40HC', 'COC', 1),
                )
                cursor.execute(
                    'INSERT INTO creation_operations '
                    '(tenant_id,id,item_id,quantity,payload) VALUES (%s,%s,%s,1,%s)',
                    (
                        ORGANIZATION,
                        OPERATION,
                        ITEM,
                        json.dumps({'tenant_id': ORGANIZATION, 'quantity': 1}),
                    ),
                )
            # Новые composite-типы тоже изолированы; рабочая public-схема теста не меняется.
            sql = migration.read_text().replace('public.', 'naming_migration.')
            cursor.execute(sql)
            cursor.execute('SELECT organization_id::text FROM organization_memberships')
            assert cursor.fetchone() == (ORGANIZATION,)
            cursor.execute('SELECT organization_id::text FROM business_audit')
            assert cursor.fetchone() == (ORGANIZATION,)
            if service == 'transportations':
                cursor.execute('SELECT document FROM business_audit')
                migrated = cursor.fetchone()[0]
                assert migrated['request']['route_segment_id'] == ITEM
                assert migrated['request']['organization_id'] == ORGANIZATION
                assert migrated['result']['route_segments'] == [{'id': ITEM}]
                assert migrated['request']['reason'] == document['request']['reason']
                assert 'legs' not in migrated['request']
                cursor.execute('SELECT payload,result FROM processed_commands')
                payload, result = cursor.fetchone()
                assert payload == {'organization_id': ORGANIZATION}
                assert result == {'route_segments': [{'id': ITEM}]}
                cursor.execute('SELECT count(*) FROM route_segments')
                assert cursor.fetchone() == (0,)
            if service == 'shipments':
                cursor.execute('SELECT payload FROM creation_operations')
                assert cursor.fetchone()[0] == {'organization_id': ORGANIZATION, 'quantity': 1}
        finally:
            cursor.execute('ROLLBACK')

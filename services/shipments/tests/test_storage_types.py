"""Типы SQL и YAML совпадают; расширение хранения не ломает старую проекцию."""

from uuid import uuid4

from postgres_types import assert_postgres_types


def test_storage_type_contract(pgsql, service_source_dir):
    assert_postgres_types(pgsql['db_1'], service_source_dir)


async def test_v3_and_future_projection_coexist(api, pgsql):
    database = pgsql['db_1']
    with database.cursor() as cursor:
        cursor.execute('''
            CREATE TYPE public.shipment_v4 AS (
                id UUID, revision BIGINT, number TEXT, customer_id UUID, reference TEXT, note TEXT
            );
            ALTER TABLE shipments ADD COLUMN note TEXT NOT NULL DEFAULT 'новое поле';
        ''')
    try:
        # Новая проекция хранения не меняет контракт используемого shipment_v3.
        created = await api.post(
            '/api/v1/shipments/create',
            {
                'customer_id': str(uuid4()),
                'reference': 'Поставка',
                'idempotency_key': str(uuid4()),
            },
            expected=201,
        )
        shipment = created['shipment']
        selected = await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']})
        assert selected['shipment'] == shipment
        with database.cursor() as cursor:
            cursor.execute(
                '''
                SELECT (ROW(id, revision, number, customer_id, reference, note)
                    ::public.shipment_v4).note
                FROM shipments WHERE id = %s::UUID
            ''',
                (shipment['id'],),
            )
            assert cursor.fetchone()[0] == 'новое поле'
        changed = await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': shipment['id'],
                'expected_revision': 1,
                'reference': 'Обновлено v2',
            },
        )
        assert changed['shipment']['revision'] == 2
        with database.cursor() as cursor:
            cursor.execute('SELECT note FROM shipments WHERE id = %s::UUID', (shipment['id'],))
            assert cursor.fetchone()[0] == 'новое поле'
    finally:
        with database.cursor() as cursor:
            cursor.execute('ALTER TABLE shipments DROP COLUMN note; DROP TYPE public.shipment_v4;')

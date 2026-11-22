"""Перенос документов маршрута в запись перевозки без потери данных."""
import json
from uuid import uuid4
from conftest import ORGANIZATION, OTHER_ORGANIZATION


def test_inline_route_migration(pgsql, service_source_dir):
    directory = service_source_dir / 'postgresql/schemas/db_1'
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('BEGIN')
        try:
            cursor.execute('CREATE SCHEMA inline_route_migration')
            cursor.execute('SET LOCAL search_path = inline_route_migration, public')
            migration = directory / 'V0023__store_route_in_transportation.sql'
            for path in sorted(directory.glob('V*.sql')):
                if path.name >= migration.name:
                    break
                cursor.execute(path.read_text().replace('public.', 'inline_route_migration.').replace('inline_route_migration.gin_trgm_ops', 'public.gin_trgm_ops'))
            identifier, empty, shipment = map(str, (uuid4(), uuid4(), uuid4()))
            documents = [
                {'id': str(uuid4()), 'mode': 'road', 'actual_departure_at': '2026-10-20T09:00:00Z',
                 'pickup': {'delivery_city': 'Москва'}, 'inherited': False},
                {'id': str(uuid4()), 'mode': 'rail', 'rail': {'etd': '2026-10-21'},
                 'contractor_id': str(uuid4())},
            ]
            for org, tid in [(ORGANIZATION, identifier), (OTHER_ORGANIZATION, identifier), (ORGANIZATION, empty)]:
                cursor.execute('INSERT INTO transportations (organization_id,id,shipment_id,ordinal,type,hazardous,revision) VALUES (%s,%s,%s,1,\'container\',FALSE,7)', (org, tid, shipment))
            # Непоследовательные позиции и обратный порядок вставки должны сохраниться корректно.
            for position in (1, 0):
                doc = documents[position]
                cursor.execute('INSERT INTO route_segments (organization_id,transportation_id,id,position,document) VALUES (%s,%s,%s,%s,%s)', (ORGANIZATION, identifier, doc['id'], position * 3, json.dumps(doc)))
            other = {'id': str(uuid4()), 'mode': 'air', 'origin': {'name': 'Другой маршрут', 'country_code': 'RU'}}
            cursor.execute('INSERT INTO route_segments VALUES (%s,%s,%s,0,%s)', (OTHER_ORGANIZATION, identifier, other['id'], json.dumps(other)))
            cursor.execute(migration.read_text().replace('public.', 'inline_route_migration.'))
            for org, tid, expected in [(ORGANIZATION, identifier, documents), (OTHER_ORGANIZATION, identifier, [other]), (ORGANIZATION, empty, [])]:
                cursor.execute('SELECT revision, route_segments FROM transportations WHERE organization_id=%s AND id=%s', (org, tid))
                assert cursor.fetchone() == (7, expected)
            cursor.execute("SELECT to_regclass('inline_route_migration.route_segments')")
            assert cursor.fetchone() == (None,)
        finally:
            cursor.execute('ROLLBACK')

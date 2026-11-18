import json
from uuid import UUID, uuid4

from conftest import ORGANIZATION


async def test_contact_id_survives_edits_and_creation_replay(api):
    body = {'id': str(uuid4()), 'name': 'Клиент', 'contacts': [
        {'name': 'Анна', 'email': 'anna@example.test'},
    ]}
    first = (await api.post('/api/v1/counterparties/create', body, expected=201))['counterparty']
    contact = first['contacts'][0]
    UUID(contact['id'])
    replay = (await api.post('/api/v1/counterparties/create', body, expected=201))['counterparty']
    assert replay == first
    changed = (await api.post('/api/v1/counterparties/update', {
        'id': first['id'], 'expected_revision': first['revision'], 'name': first['name'],
        'contacts': [{**contact, 'email': 'new@example.test', 'phone': '+7 999 123-45-67'}],
    }))['counterparty']
    assert changed['contacts'][0]['id'] == contact['id']
    assert changed['contacts'][0]['email'] == 'new@example.test'


async def test_duplicate_contact_id_is_rejected(api):
    identifier = str(uuid4())
    await api.post('/api/v1/counterparties/create', {
        'id': str(uuid4()), 'name': 'Клиент',
        'contacts': [{'id': identifier, 'name': 'Анна'}, {'id': identifier, 'name': 'Борис'}],
    }, expected=400)


async def test_existing_contact_identity_migration_is_repeatable(pgsql, service_source_dir):
    migration = (service_source_dir / 'postgresql/schemas/db_1/V0015__contact_identity.sql').read_text()
    identifier = str(uuid4())
    existing = str(uuid4())
    contacts = [{'name': 'Первый'}, {'id': existing, 'name': 'Второй'}]
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('INSERT INTO counterparties (organization_id, id, name, contacts) VALUES (%s,%s,%s,%s::JSONB)',
                       (ORGANIZATION, identifier, 'Миграция контактов', json.dumps(contacts)))
        cursor.execute(migration)
        cursor.execute('SELECT contacts FROM counterparties WHERE organization_id=%s AND id=%s',
                       (ORGANIZATION, identifier))
        first = cursor.fetchone()[0]
        assert [contact['name'] for contact in first] == ['Первый', 'Второй']
        UUID(first[0]['id'])
        assert first[1]['id'] == existing
        cursor.execute(migration)
        cursor.execute('SELECT contacts FROM counterparties WHERE organization_id=%s AND id=%s',
                       (ORGANIZATION, identifier))
        assert cursor.fetchone()[0] == first

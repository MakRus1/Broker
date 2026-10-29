"""Банковские счета: отдельные права, изоляция владельцев, редакции и архив."""
import asyncio
from uuid import uuid4

import pytest

READ = ['counterparties.read', 'counterparties.read_bank_accounts']
WRITE = [*READ, 'counterparties.write_bank_accounts']
BASE = '/api/v1/counterparties/bank-accounts/'

async def owner(api, **fields):
    return (await api.post('/api/v1/counterparties/create',
        {'id': str(uuid4()), 'name': 'Наше юридическое лицо', **fields}, expected=201))['counterparty']

def account(party, **fields):
    return {'id': str(uuid4()), 'counterparty_id': party['id'], 'expected_revision': 0,
        'name': ' Основной рублёвый ', 'number': ' 40702 8100 00000000001 ',
        'currency': 'rub', 'bank_name': ' Банк ', 'bic': '044525225',
        'is_primary': True, 'archived': False, **fields}

async def save(api, body, **kwargs):
    return await api.post(BASE+'save', body, roles=WRITE, **kwargs)

async def test_own_company_is_revisioned_and_filterable(api):
    party = await owner(api, own_company=True)
    assert party['own_company'] is True
    await owner(api)
    result = await api.post('/api/v1/counterparties/list',
        {'filters': [{'field':'own_company', 'values':['true']}]})
    assert [row['id'] for row in result['items']] == [party['id']]
    changed = (await api.post('/api/v1/counterparties/update',
        {'id':party['id'], 'name':party['name'], 'expected_revision':1, 'own_company':False}))['counterparty']
    assert changed['own_company'] is False
    assert changed['revision'] == 2
    history = await api.post('/api/v1/counterparties/history', {'id':party['id']})
    assert history['items'][0]['after']['own_company'] is False

async def test_account_lifecycle_and_separate_permissions(api, pgsql):
    party = await owner(api)
    body = account(party)
    await api.post(BASE+'save', body, roles=['counterparties.write'], expected=403)
    saved = (await save(api, body))['account']
    assert saved['name'] == 'Основной рублёвый'
    assert saved['number'] == '40702810000000000001'
    assert saved['bank_name'] == 'Банк'
    assert saved['currency'] == 'RUB'
    assert saved['revision'] == 1
    await api.post(BASE+'list', {'counterparty_id':party['id']}, roles=['counterparties.read'], expected=403)
    listed = await api.post(BASE+'list', {'counterparty_id':party['id']}, roles=READ)
    assert listed['items'] == [saved]
    assert (await api.post('/api/v1/counterparties/get', {'id':party['id']}))['counterparty']['revision'] == 1
    changed = {**body, 'expected_revision':1, 'archived':True, 'is_primary':False}
    archived = (await save(api, changed))['account']
    assert archived['archived'] is True and archived['revision'] == 2
    await save(api, changed, expected=409)
    restored = (await save(api, {**changed, 'expected_revision':2, 'archived':False}))['account']
    assert restored['id'] == saved['id'] and restored['revision'] == 3
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM business_audit WHERE entity_id=%s', (saved['id'],))
        assert cursor.fetchone()[0] == 3

async def test_accounts_do_not_cross_organizations_or_owners(api):
    party = await owner(api)
    body = account(party)
    await save(api, body)
    await api.post(BASE+'list', {'counterparty_id':party['id']}, roles=READ, user='other', expected=404)
    await save(api, body, user='other', expected=404)
    another = await owner(api)
    await save(api, {**body, 'counterparty_id':another['id'], 'expected_revision':1}, expected=404)

async def test_primary_and_duplicate_numbers_are_scoped_by_currency(api):
    party = await owner(api)
    body = account(party)
    await save(api, body)
    await save(api, {**body, 'id':str(uuid4()), 'number':'another'}, expected=409)
    await save(api, {**body, 'id':str(uuid4()), 'is_primary':False}, expected=409)
    await save(api, {**body, 'id':str(uuid4()), 'currency':'USD'})
    await save(api, {**body, 'id':str(uuid4()), 'number':'another', 'is_primary':False})

async def test_concurrent_primary_accounts_do_not_race(api):
    party = await owner(api)
    first = account(party)
    second = account(party, number='second')
    results = await asyncio.gather(save(api, first, expected=(200,409)), save(api, second, expected=(200,409)))
    assert sum('account' in result for result in results) == 1

@pytest.mark.parametrize('fields', [{'name':'  '}, {'number':'  '}, {'bank_name':'  '},
    {'bic':'123'}, {'swift':'BAD'}, {'currency':'R'}, {'archived':True, 'is_primary':True}])
async def test_invalid_requisites(api, fields):
    party = await owner(api)
    await save(api, account(party, **fields), expected=400)

async def test_archived_owner_cannot_change_accounts(api):
    party = await owner(api)
    await api.post('/api/v1/counterparties/set-archived',
        {'id':party['id'], 'expected_revision':1, 'archived':True})
    await save(api, account(party), expected=409)


async def test_same_account_id_cannot_race_between_owners(api):
    first_owner = await owner(api)
    second_owner = await owner(api)
    first = account(first_owner)
    second = {**first, 'counterparty_id': second_owner['id']}
    results = await asyncio.gather(save(api, first, expected=(200,404,409)), save(api, second, expected=(200,404,409)))
    assert sum('account' in result for result in results) == 1


async def test_account_uuid_case_and_revision_overflow(api):
    party = await owner(api)
    body = account(party)
    await save(api, body)
    updated = (await save(api, {**body, 'counterparty_id': party['id'].upper(),
        'id': body['id'].upper(), 'expected_revision': 1}))['account']
    assert updated['id'] == body['id']
    assert updated['revision'] == 2
    await save(api, {**body, 'expected_revision': 9223372036854775807}, expected=400)

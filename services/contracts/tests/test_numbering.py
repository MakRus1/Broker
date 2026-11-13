"""Нумерация договоров принадлежит организации, параллельные создания не дублируют номера."""
import asyncio
import pytest
from conftest import ORGANIZATION
from test_contracts import contract

@pytest.fixture
def administrator(mockserver):
    state={'allowed':True}
    @mockserver.json_handler('/api/organizations/administration/check')
    def check(request):
        if not state['allowed']: return mockserver.make_response(status=403,json={'error':'forbidden','message':'Нет доступа'})
        return {'organization_id':request.json['organization_id']}
    return state

async def test_contract_numbering_masks_and_concurrency(api,directory,administrator):
    base='/api/admin/contracts/numbering/'
    assert (await api.post(base+'get',{'organization_id':ORGANIZATION}))['number_template'] == 'Д-{sequence}/{yyyy}'
    rules={'organization_id':ORGANIZATION,'number_template':'ABC{sequence}/{dd}.{mm}.{yyyy}',
        'reset_number_yearly':True,'expected_revision':0}
    await api.post(base+'update',rules)
    async def create(date='2026-10-30'):
        return (await api.post('/api/v1/contracts/save',{**contract(),'number':None,'contract_date':date,'valid_until':None}))['contract']
    first,second = await asyncio.gather(create(),create())
    assert {first['number'],second['number']} == {'ABC1/30.10.2026','ABC2/30.10.2026'}
    assert (await create('2027-01-01'))['number'] == 'ABC1/01.01.2027'
    await api.post(base+'update',rules,expected=409)
    await api.post(base+'update',{**rules,'expected_revision':1,'number_template':'{bad}'},expected=400)
    administrator['allowed']=False
    await api.post(base+'get',{'organization_id':ORGANIZATION},expected=403)


async def test_numbering_update_checks_administrator_before_writing(api, administrator, pgsql):
    administrator['allowed'] = False
    await api.post('/api/admin/contracts/numbering/update', {
        'organization_id': ORGANIZATION, 'number_template': 'X-{sequence}',
        'reset_number_yearly': False, 'expected_revision': 0,
    }, expected=403)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT COUNT(*) FROM contract_numbering WHERE organization_id = %s::UUID',
                       (ORGANIZATION,))
        assert cursor.fetchone()[0] == 0

import uuid
import pytest
from conftest import OWN, PARTY, ACCOUNT


def contract(**changes):
    return dict(id=str(uuid.uuid4()), own_company_id=OWN, counterparty_id=PARTY,
                number='Д-1',contract_date='2026-10-30',valid_until=None,currency='RUB',
                own_bank_account_id=None,counterparty_bank_account_id=None,
                payment_delay_days=None,note=None,archived=False,expected_revision=0,**changes)


async def test_create_get_update_and_revision_conflict(api, directory, pgsql):
    body = contract()
    body.update(number='  Д-1  ',payment_delay_days=30,own_bank_account_id=ACCOUNT)
    saved = (await api.post('/api/v1/contracts/save',body))['contract']
    assert saved['number'] == 'Д-1'
    assert saved['revision'] == 1
    assert saved['payment_delay_days'] == 30
    assert (await api.post('/api/v1/contracts/get',{'id':body['id']}))['contract'] == saved
    body.update(expected_revision=1,payment_delay_days=0,note=' Текст ')
    updated = (await api.post('/api/v1/contracts/save',body))['contract']
    assert updated['revision'] == 2
    assert updated['payment_delay_days'] == 0
    assert updated['note'] == 'Текст'
    await api.post('/api/v1/contracts/save',body,expected=409)
    cursor=pgsql['db_1'].cursor()
    cursor.execute('SELECT COUNT(*) FROM contract_history WHERE contract_id=%s',(body['id'],))
    assert cursor.fetchone()[0] == 2


@pytest.mark.parametrize('field,value',[
    ('number','   '),('contract_date','1111-01-01'),('valid_until','2026-01-01'),
    ('payment_delay_days',-1),('payment_delay_days',1.5),('payment_delay_days',36501),
    ('counterparty_id',OWN),('currency','rub'),('contract_date','2026-02-30'),
])
async def test_invalid_fields(api,directory,field,value):
    body=contract();body[field]=value
    await api.post('/api/v1/contracts/save',body,expected=400)


@pytest.mark.parametrize('change,expected',[
    ({'own_company':False},400),({'archived':True},409),({'status':404},404),
    ({'status':403},403),({'status':500},502),
])
async def test_directory_validation(api,directory,change,expected):
    directory.update(change)
    await api.post('/api/v1/contracts/save',contract(),expected=expected)


@pytest.mark.parametrize('change,account',[
    ({'currency':'USD'},ACCOUNT),({'account_archived':True},ACCOUNT),({},str(uuid.uuid4())),
])
async def test_invalid_bank_account(api,directory,change,account):
    directory.update(change)
    body=contract();body['own_bank_account_id']=account
    await api.post('/api/v1/contracts/save',body,expected=400)


async def test_duplicate_id_and_organization_isolation(api,directory):
    body=contract()
    await api.post('/api/v1/contracts/save',body)
    await api.post('/api/v1/contracts/save',body,expected=409)
    await api.post('/api/v1/contracts/get',{'id':body['id']},user='other',expected=404)
    assert not (await api.post('/api/v1/contracts/list',{},user='other'))['items']


async def test_pagination_search_archive_and_nullable_delay(api,directory):
    for number in ['ABC-1','ABC-2','ZZZ']:
        body=contract();body['number']=number
        value=(await api.post('/api/v1/contracts/save',body))['contract']
        assert value['payment_delay_days'] is None
    first=await api.post('/api/v1/contracts/list',{'filters':[{'field':'number','query':'bc'}],'limit':1})
    assert len(first['items']) == 1 and first['next_cursor']
    second=await api.post('/api/v1/contracts/list',{'filters':[{'field':'number','query':'bc'}],'limit':1,'cursor':first['next_cursor']})
    assert len(second['items']) == 1 and not second.get('next_cursor')
    assert first['items'][0]['id'] != second['items'][0]['id']
    value=first['items'][0]
    body={key:item for key,item in value.items() if key!='revision'}
    body.update(archived=True,expected_revision=value['revision'])
    await api.post('/api/v1/contracts/save',body)
    assert len((await api.post('/api/v1/contracts/list',{'filters':[{'field':'archived','values':['true']}]}))['items']) == 1


async def test_missing_permissions(api,directory):
    await api.post('/api/v1/contracts/list',{},roles=['counterparties.read'],expected=403)
    await api.post('/api/v1/contracts/save',contract(),roles=['contracts.read','counterparties.read'],expected=403)


async def test_registry_filters_and_sorting(api,directory):
    for number,when,until in [('Д-10','2026-10-30',None),('Д-2','2026-10-29','2027-01-01')]:
        body=contract();body.update(number=number,contract_date=when,valid_until=until)
        await api.post('/api/v1/contracts/save',body)
    listing=await api.post('/api/v1/contracts/list',{'sort':'number'})
    assert [item['number'] for item in listing['items']] == ['Д-2','Д-10']
    result=await api.post('/api/v1/contracts/list',{'filters':[
        {'field':'own_company_id','values':[OWN]},
        {'field':'counterparty_id','values':[PARTY]},
        {'field':'contract_date','date_from':'2026-10-29','date_to':'2026-10-29'},
        {'field':'valid_until','date_from':'2027-01-01'},
    ]})
    assert [item['number'] for item in result['items']] == ['Д-2']
    result=await api.post('/api/v1/contracts/list',{'sort':'valid_until','filters':[{'field':'valid_until','empty':True}]})
    assert [item['number'] for item in result['items']] == ['Д-10']
    fields=(await api.post('/api/v1/contracts/filter-values',{'registry':{}}))['fields']
    assert next(field for field in fields if field['id']=='contract_date')['kind'] == 'date'
    assert next(field for field in fields if field['id']=='counterparty_id')['lookup'] == 'counterparties'
    values=(await api.post('/api/v1/contracts/filter-values',{'registry':{},'field':'number'}))['values']
    assert {value['value'] for value in values} == {'Д-2','Д-10'}
    await api.post('/api/v1/contracts/list',{'sort':'unknown'},expected=400)
    await api.post('/api/v1/contracts/list',{'filters':[{'field':'unknown','values':['1']}]},expected=400)
    first=await api.post('/api/v1/contracts/list',{'limit':1,'sort':'contract_date'})
    await api.post('/api/v1/contracts/list',{'limit':1,'sort':'number','cursor':first['next_cursor']},expected=400)

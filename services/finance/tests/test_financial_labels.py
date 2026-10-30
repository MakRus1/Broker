from uuid import uuid4
import asyncio
import pytest

BASE='/api/v1/financial-labels/'


def label(**changes):
    return dict({'id':str(uuid4()),'name':'Морской фрахт','direction':'both','unit':'container',
                'currency':'RUB','vat_mode':'exempt','vat_rate_basis_points':None,
                'note':None,'archived':False,'expected_revision':0},**changes)


async def test_create_get_update_and_history(api,pgsql):
    body=label(name='  Фрахт  ',note='  Примечание  ')
    result=(await api.post(BASE+'save',body))['label']
    assert result['name']=='Фрахт' and result['note']=='Примечание' and result['revision']==1
    assert (await api.post(BASE+'get',{'id':body['id']}))['label']==result
    body.update(expected_revision=1,name='Доставка',note=' ',unit='transportation')
    saved=(await api.post(BASE+'save',body))['label']
    assert saved['revision']==2 and saved['unit']=='transportation' and saved['note'] is None
    await api.post(BASE+'save',body,expected=409)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT revision, snapshot FROM financial_label_history WHERE label_id=%s ORDER BY revision',(body['id'],))
        history=cursor.fetchall()
    assert [row[0] for row in history]==[1,2]
    assert history[1][1]['name']=='Доставка'


@pytest.mark.parametrize('mode,rate,expected',[
    ('exempt',None,200),('exempt',0,400),('rate',None,400),('rate',0,200),
    ('rate',2250,200),('rate',10000,200),('rate',10001,400),('rate',-1,400),('rate',1.5,400),
])
async def test_vat_representation(api,mode,rate,expected):
    result=await api.post(BASE+'save',label(vat_mode=mode,vat_rate_basis_points=rate),expected=expected)
    if expected==200:
        assert result['label']['vat_mode']==mode
        assert result['label']['vat_rate_basis_points']==rate


@pytest.mark.parametrize('change',[
    {'name':' '},{'direction':'unknown'},{'unit':'unknown'},{'currency':'rub'},
    {'vat_mode':'unknown'},{'expected_revision':9223372036854775807},
])
async def test_invalid_fields(api,change):
    await api.post(BASE+'save',label(**change),expected=400)


async def test_organization_isolation_and_permissions(api):
    body=label()
    await api.post(BASE+'save',body)
    await api.post(BASE+'get',{'id':body['id']},user='other',expected=404)
    assert not (await api.post(BASE+'list',{},user='other'))['items']
    await api.post(BASE+'list',{},roles=[],expected=403)
    await api.post(BASE+'save',body,roles=['finance.read_financial_labels'],expected=403)
    await api.post(BASE+'filter-values',{'registry':{}},roles=[],expected=403)
    await api.post(BASE+'list',{},roles=['finance.write_financial_labels'],expected=403)


async def test_concurrent_updates_return_one_conflict(api):
    body=label()
    await api.post(BASE+'save',body)
    body['expected_revision']=1
    results=await asyncio.gather(*[api.post(BASE+'save',{**body,'name':name},expected=(200,409)) for name in ['Один','Два']])
    assert sum('label' in result for result in results)==1
    assert (await api.post(BASE+'get',{'id':body['id']}))['label']['revision']==2


async def test_registry_filters_pagination_archive_restore(api):
    saved=[]
    for name,direction,unit,currency in [('Альфа','income','container','USD'),('Бета','expense','day','RUB'),('Гамма','both','container','USD')]:
        body=label(name=name,direction=direction,unit=unit,currency=currency)
        saved.append((await api.post(BASE+'save',body))['label'])
    first=await api.post(BASE+'list',{'sort':'name','limit':1})
    assert first['items'][0]['name']=='Альфа' and first['next_cursor']
    second=await api.post(BASE+'list',{'sort':'name','limit':1,'cursor':first['next_cursor']})
    assert second['items'][0]['name']=='Бета'
    filters=[{'field':'unit','values':['container']},{'field':'currency','values':['USD']},{'field':'name','query':'амм'}]
    found=await api.post(BASE+'list',{'sort':'name','filters':filters})
    assert [item['name'] for item in found['items']]==['Гамма']
    await api.post(BASE+'list',{'sort':'currency','cursor':first['next_cursor']},expected=400)
    await api.post(BASE+'list',{'sort':'unknown'},expected=400)
    directions=await api.post(BASE+'list',{'sort':'direction'})
    assert [item['direction'] for item in directions['items']]==['income','both','expense']
    fields=(await api.post(BASE+'filter-values',{'registry':{}}))['fields']
    assert {field['id'] for field in fields}=={'name','direction','unit','currency','vat_mode','archived'}
    values=(await api.post(BASE+'filter-values',{'registry':{},'field':'direction'}))['values']
    assert {value['label'] for value in values}=={'Доход','Расход','Доход и расход'}
    value=saved[0];body={key:item for key,item in value.items() if key!='revision'}
    body.update(expected_revision=1,archived=True)
    await api.post(BASE+'save',body)
    active=await api.post(BASE+'list',{'filters':[{'field':'archived','values':['false']}]})
    assert len(active['items'])==2
    body.update(expected_revision=2,archived=False)
    restored=(await api.post(BASE+'save',body))['label']
    assert restored['id']==value['id'] and restored['revision']==3

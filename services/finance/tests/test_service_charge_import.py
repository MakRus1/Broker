import asyncio
import copy
from uuid import uuid4
from conftest import ISSUER, ORGANIZATION
from test_financial_labels import label

PATH='/api/internal/v1/service-charges/apply-import'

async def test_import_is_atomic_idempotent_and_service_only(api,service_client,auth_key,pgsql):
    saved=(await api.post('/api/v1/financial-labels/save',label()))['label']
    shipment=str(uuid4())
    item={'charge':{'id':str(uuid4()),'shipment_id':shipment,'transportation_id':None,
        'label_id':saved['id'],'direction':'income','unit':'container','counterparty_id':str(uuid4()),
        'contract_id':None,'quantity_milli':1000,'price_minor':12345,'currency':'RUB',
        'vat_mode':'rate','vat_rate_basis_points':2000,'note':None,'archived':False,'price_mode':'net',
        'expected_revision':0,'service_date':'2026-11-08','invoice_currency':'RUB','manual_invoice_rate_millionths':None},
        'label_name':'Зафиксированное имя','counterparty_name':'Зафиксированный клиент'}
    batch={'organization_id':ORGANIZATION,'id':str(uuid4()),'actor_subject':'author','actor_issuer':ISSUER,
           'source':{'type':'sales.proposal','id':str(uuid4()),'revision':3},'items':[item]}
    token=auth_key.token('service-account-sales',roles=['finance.import_service_charges'],claims={'azp':'sales-dispatcher'})
    async def send(body,access=token):
        return await service_client.post(PATH,json=body,headers={'Authorization':'Bearer '+access})
    responses=await asyncio.gather(*(send(batch) for _ in range(4)))
    assert [r.status for r in responses]==[200]*4,[r.text for r in responses]
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM service_charge_imports')
        assert cursor.fetchone()[0]==1
        cursor.execute('SELECT label_name, counterparty_name, net_minor, vat_minor, total_minor FROM service_charges')
        assert cursor.fetchall()==[('Зафиксированное имя','Зафиксированный клиент',12345,2469,14814)]
        # Re-delivery must not revert subsequent user edits.
        cursor.execute('UPDATE service_charges SET note=%s, revision=2',('Изменено пользователем',))
    assert (await send(batch)).status==200
    changed=copy.deepcopy(batch);changed['items'][0]['charge']['price_minor']=500
    assert (await send(changed)).status==409
    forged=auth_key.token('author',roles=['finance.import_service_charges'],claims={'azp':'broker-web'})
    assert (await send(batch,forged)).status==403
    assert (await send({**batch,'actor_issuer':'https://other.invalid'})).status==400
    invalid=copy.deepcopy(batch);invalid['id']=str(uuid4());invalid['items'][0]['charge']['id']=str(uuid4())
    invalid['items'].append(copy.deepcopy(invalid['items'][0]))
    invalid['items'][1]['charge']['id']=str(uuid4());invalid['items'][1]['charge']['label_id']=str(uuid4())
    assert (await send(invalid)).status==404
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM service_charge_imports')
        assert cursor.fetchone()[0]==1
        cursor.execute('SELECT note, revision FROM service_charges')
        assert cursor.fetchall()==[('Изменено пользователем',2)]

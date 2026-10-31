"""Курсы по дате услуги, валюты счёта, неизменность снимков и настройки организации."""
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION
from test_service_charges import references, scope, charge
from test_invoices import invoice, invoice_references

RATES='/api/v1/exchange-rates/get'
SETTINGS='/api/admin/finance/settings/'
NUMBERING = {
    'outgoing_number_template': 'ИС-{sequence}/{yyyy}',
    'incoming_number_template': 'ВС-{sequence}/{yyyy}',
    'reset_numbers_yearly': False,
}

@pytest.fixture
def finance_admin(mockserver):
    state={'allowed':True}
    @mockserver.json_handler('/api/organizations/administration/check')
    def check(request):
        if not state['allowed']:
            return mockserver.make_response(status=403,json={'error':'forbidden','message':'Нет доступа'})
        return {'organization_id':request.json['organization_id']}
    return state

async def test_cbr_cross_rate_nominal_and_persistent_cache(api,cbr):
    query={'date':'2026-10-30','from_currency':'USD','to_currency':'JPY'}
    result=await api.post(RATES,query)
    assert result=={'date':'2026-10-30','rate_millionths':150000000,'source':'cbr'}
    cbr['rates']['USD']=(1,'100,0000')
    assert await api.post(RATES,query)==result
    assert cbr['handler'].times_called==1
    result=await api.post(RATES,{**query,'from_currency':'JPY','to_currency':'USD'})
    assert result['rate_millionths']==6667
    cbr['date']='30.10.2026'
    weekend=await api.post(RATES,{**query,'date':'2026-10-31','to_currency':'RUB'})
    assert weekend['date']=='2026-10-30'
    assert weekend['rate_millionths']==100000000

async def test_missing_invalid_and_identity_rates(api,cbr):
    body={'date':'2026-10-30','from_currency':'RUB','to_currency':'RUB'}
    assert (await api.post(RATES,body))['rate_millionths']==1000000
    assert cbr['handler'].times_called==0
    await api.post(RATES,{**body,'from_currency':'XYZ'},expected=503)
    await api.post(RATES,{**body,'date':'1111-01-01'},expected=400)
    await api.post(RATES,{**body,'from_currency':'rub'},expected=400)

async def test_invoice_uses_conversion_and_preserves_snapshot(api,charge,invoice_references,cbr):
    body=await charge(currency='USD',invoice_currency='RUB')
    saved=(await api.post('/api/v1/service-charges/save',body))['charge']
    assert saved['total_minor']==12000
    assert saved['invoice_total_minor']==1080000
    assert saved['invoice_net_minor']+saved['invoice_vat_minor']==saved['invoice_total_minor']
    document=invoice([saved['id']])
    result=await api.post('/api/v1/invoices/save',document)
    assert result['invoice']['total_minor']==1080000
    assert result['charges'][0]['invoice_rate']['rate_millionths']==90000000
    cbr['status']=503
    assert (await api.post('/api/v1/invoices/get',{'id':document['id']}))==result
    changed=await api.post('/api/v1/service-charges/save',{**body,'expected_revision':1,'manual_invoice_rate_millionths':95000000})
    assert changed['charge']['invoice_total_minor']==1140000
    assert (await api.post('/api/v1/invoices/get',{'id':document['id']}))['invoice']['total_minor']==1140000

async def test_manual_rate_without_cbr_and_rounding(api,charge,invoice_references,cbr):
    cbr['status']=503
    body=await charge(currency='USD',invoice_currency='RUB',manual_invoice_rate_millionths=1250000,
        price_minor=1,quantity_milli=1000,vat_rate_basis_points=2000)
    saved=(await api.post('/api/v1/service-charges/save',body))['charge']
    assert saved['invoice_rate']['source']=='manual'
    assert saved['invoice_total_minor']==1
    assert cbr['handler'].times_called==0
    await api.post('/api/v1/service-charges/save',{**body,'expected_revision':1,'currency':'RUB'},expected=400)

async def test_reporting_currency_permissions_cas_and_no_double_count(api,charge,invoice_references,finance_admin,scope):
    before=await api.post(SETTINGS+'get',{'organization_id':ORGANIZATION})
    assert before=={**NUMBERING,'reporting_currency':'RUB','revision':0}
    await api.post(SETTINGS+'update',{**NUMBERING,'organization_id':ORGANIZATION,'reporting_currency':'USD','expected_revision':9},expected=409)
    configured=await api.post(SETTINGS+'update',{**NUMBERING,'organization_id':ORGANIZATION,'reporting_currency':'USD','expected_revision':0})
    assert configured['revision']==1
    await api.post(SETTINGS+'update',{**NUMBERING,'organization_id':ORGANIZATION,'reporting_currency':'CNY','expected_revision':0},expected=409)
    body=await charge(currency='USD',invoice_currency='RUB')
    await api.post('/api/v1/service-charges/save',body)
    rows=await api.post('/api/v1/service-charges/list',{'scope':scope,'after':None,'limit':50,'include_archived':False})
    assert rows['reporting']['currency']=='USD'
    assert rows['reporting']['missing_rates']==[]
    assert rows['reporting']['summary']['combined']==[{'currency':'USD','income_minor':'12000','expense_minor':'0'}]
    finance_admin['allowed']=False
    await api.post(SETTINGS+'update',{**NUMBERING,'organization_id':OTHER_ORGANIZATION,'reporting_currency':'USD','expected_revision':0},expected=403)
    await api.post(SETTINGS+'get',{'organization_id':OTHER_ORGANIZATION},expected=403)

async def test_reporting_missing_rate_is_explicit(api,charge,invoice_references,finance_admin,cbr,scope):
    await api.post(SETTINGS+'update',{**NUMBERING,'organization_id':ORGANIZATION,'reporting_currency':'XYZ','expected_revision':0})
    body=await charge()
    await api.post('/api/v1/service-charges/save',body)
    rows=await api.post('/api/v1/service-charges/list',{'scope':scope,'after':None,'limit':50,'include_archived':False})
    assert rows['reporting']['missing_rates']==[{'currency':'RUB','date':'2026-10-30'}]
    assert rows['reporting']['summary']['combined']==[]
    assert rows['summary']['combined'][0]['income_minor']=='12000'

@pytest.mark.parametrize('rate,expected', [(500000,1),(499999,0),(1500000,2),(2000000,2)])
async def test_conversion_rounds_half_up_in_minor_units(api,charge,invoice_references,rate,expected):
    body=await charge(currency='USD',invoice_currency='RUB',manual_invoice_rate_millionths=rate,
        price_minor=1,quantity_milli=1000,vat_rate_basis_points=0)
    saved=(await api.post('/api/v1/service-charges/save',body))['charge']
    assert saved['invoice_total_minor']==expected
    assert saved['invoice_net_minor']+saved['invoice_vat_minor']==expected

async def test_conversion_overflow_is_rejected_before_storage(api,charge,invoice_references):
    body=await charge(currency='USD',invoice_currency='RUB',manual_invoice_rate_millionths=1000000000000,
        price_minor=100000000,quantity_milli=10000000)
    await api.post('/api/v1/service-charges/save',body,expected=400)
    await api.post('/api/v1/service-charges/get',{'id':body['id']},expected=404)

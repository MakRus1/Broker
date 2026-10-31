"""Серверный реестр услуг: комбинации фильтров, стабильные страницы и фасеты."""
import pytest
from test_service_charges import charge, references, scope
from test_invoices import invoice, invoice_references

BASE = '/api/v1/service-charges/'

async def seed(api, charge):
    result=[]
    for date,price in [('2026-10-29',300),('2026-10-30',100),('2026-10-30',200)]:
        body=await charge(shipment_id=None,service_date=date,price_minor=price)
        result.append((await api.post(BASE+'save',body))['charge'])
    return result

@pytest.mark.parametrize('sort',['service_date','label_name','net_minor','vat_minor','total_minor','invoice_number'])
@pytest.mark.parametrize('descending',[False,True])
async def test_registry_pages_have_no_gaps_or_duplicates(api,charge,sort,descending):
    saved=await seed(api,charge)
    query={'sort':sort,'descending':descending,'limit':1}
    seen=[]
    for _ in range(4):
        page=await api.post(BASE+'registry',query)
        seen.extend(item['id'] for item in page['items'])
        if 'next_cursor' not in page: break
        query['cursor']=page['next_cursor']
    assert len(seen)==3 and set(seen)=={item['id'] for item in saved}
    expected=sorted(saved,key=lambda item:(item[sort] if sort in item else '',item['id']),reverse=descending)
    assert seen==[item['id'] for item in expected]

async def test_combined_filters_and_cursor_scope(api,charge):
    saved=await seed(api,charge)
    query={'sort':'service_date','limit':1,'filters':[
        {'field':'service_date','date_from':'2026-10-30','date_to':'2026-10-30'},
        {'field':'direction','values':['income']}, {'field':'currency','values':['RUB']}]}
    page=await api.post(BASE+'registry',query)
    assert page['items'][0]['id'] in {item['id'] for item in saved[1:]}
    await api.post(BASE+'registry',{**query,'cursor':page['next_cursor'],'descending':False},expected=400)
    assert (await api.post(BASE+'registry',query,user='other'))['items']==[]
    await api.post(BASE+'registry',{'sort':'unknown'},expected=400)
    await api.post(BASE+'registry',{'filters':[{'field':'unknown','query':'x'}]},expected=400)

async def test_invoice_filter_and_facets(api,charge,invoice_references):
    saved=await seed(api,charge)
    document=(await api.post('/api/v1/invoices/save',{**invoice([saved[0]['id']]),'number':'РЕЕСТР-42'}))['invoice']
    query={'filters':[{'field':'invoice_number','query':'естр-4'}]}
    page=await api.post(BASE+'registry',query)
    assert [item['id'] for item in page['items']]==[saved[0]['id']]
    assert page['invoices'][0]['invoice_id']==document['id']
    assert len((await api.post(BASE+'registry',{'filters':[{'field':'invoice_number','query':'ЕСТР-4'}]}))['items'])==1
    unassigned=await api.post(BASE+'registry',{'filters':[{'field':'invoice_number','empty':True}]})
    assert len(unassigned['items'])==2
    facets=await api.post(BASE+'filter-values',{'registry':query,'field':'direction'})
    assert facets['values']==[{'value':'income','label':'Доход'}]
    assert (await api.post(BASE+'filter-values',{'registry':query,'field':'direction'},user='other'))['values']==[]
    catalog=await api.post(BASE+'filter-values',{'registry':{}})
    assert {'service_date','counterparty_id','invoice_number','shipment_id','transportation_id'} <= {f['id'] for f in catalog['fields']}

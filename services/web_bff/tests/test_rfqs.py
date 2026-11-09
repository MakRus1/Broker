import pytest
from uuid import uuid4

@pytest.mark.parametrize('action', ['save','get','list','history'])
async def test_rfq_proxy_preserves_versions_and_session(browser, mockserver, action):
    await browser.sign_in()
    rfq_id,inquiry_id,segment_id=[str(uuid4()) for _ in range(3)]
    item={'revision':2,'inquiry_number':'ENQ-1','route_segment':{'id':segment_id,'type':'sea'},
          'input':{'id':rfq_id,'inquiry_id':inquiry_id,'route_segment_id':segment_id,
                   'title':'Фрахт','responsible_subject':'author','status':'in_progress','quotes':[]}}
    body={**item['input'],'expected_revision':1} if action=='save' else {'id':inquiry_id if action=='list' else rfq_id}
    if action=='history':body['after_revision']=1
    expected={'items':[{'snapshot':item,'created_at':'2026-11-08T12:00:00+00:00','actor':'issuer#author'}]} if action=='history' else {'items':[item]} if action=='list' else {'item':item}
    @mockserver.json_handler('/api/v1/sales/rfqs/'+action)
    def endpoint(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json==body
        return expected
    response=await browser.post('/api/v1/sales/rfqs/'+action,body)
    assert response.status==200
    result=response.json()
    assert result==expected

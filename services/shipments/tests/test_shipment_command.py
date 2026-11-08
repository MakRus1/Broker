import asyncio
from uuid import uuid4
from conftest import ISSUER, ORGANIZATION

PATH='/api/internal/v1/shipments/ensure-created'

async def test_command_is_generic_idempotent_and_transactional(service_client, auth_key, pgsql):
    command={
        'organization_id':ORGANIZATION,'actor_subject':'author','actor_issuer':ISSUER,
        'source':{'type':'sales.inquiry','id':str(uuid4())},
        'shipment':{'customer_id':str(uuid4()),'idempotency_key':str(uuid4()),'reference':'ENQ-12','logist':'logist-author','route_segments':[]}}
    token=auth_key.token('service-account-sales',roles=['shipments.create'],claims={'azp':'sales-dispatcher'})
    async def send(value):
        return await service_client.post(PATH,json=value,headers={'Authorization':'Bearer '+token})
    responses=await asyncio.gather(*(send(command) for _ in range(4)))
    assert [response.status for response in responses]==[200]*4, [r.text for r in responses]
    ids={response.json()['shipment']['id'] for response in responses}
    assert len(ids)==1
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM shipment_sources')
        assert cursor.fetchone()[0]==1
        cursor.execute("SELECT count(*) FROM outbox WHERE topic='notifications.publish' AND payload->>'type'='shipment.created'")
        assert cursor.fetchone()[0]==1
    conflicting={**command,'shipment':{**command['shipment'],'reference':'Changed'}}
    assert (await send(conflicting)).status==409
    forged=auth_key.token('author',roles=['shipments.create'],claims={'azp':'broker-web'})
    assert (await service_client.post(PATH,json=command,headers={'Authorization':'Bearer '+forged})).status==403
    assert (await send({**command,'actor_issuer':'https://other-issuer.invalid'})).status==400

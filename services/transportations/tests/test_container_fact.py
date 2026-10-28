"""Агрегаты учитывают все контейнерные перевозки и изолированы организацией и поставкой."""
from uuid import uuid4
from conftest import ORGANIZATION, OTHER_ORGANIZATION


async def test_fact_groups_and_pending_materialization(api, pgsql):
    shipment, other_shipment, operation = (str(uuid4()) for _ in range(3))
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("INSERT INTO processed_commands (organization_id, operation_id, payload, result) VALUES (%s,%s,'{}','{}')", (ORGANIZATION,operation))
        for organization, sid, code, owner, kind, oid in [
            (ORGANIZATION, shipment, '40HC', 'COC', 'container', operation),
            (ORGANIZATION, shipment, '40HC', 'COC', 'container', None),
            (ORGANIZATION, shipment, '20DC', 'SOC', 'container', None),
            (ORGANIZATION, shipment, None, None, 'container', None),
            (ORGANIZATION, shipment, None, None, 'air', None),
            (OTHER_ORGANIZATION, shipment, '40HC', 'COC', 'container', None),
            (ORGANIZATION, other_shipment, '40HC', 'COC', 'container', None),
        ]:
            cursor.execute('''INSERT INTO transportations
                (organization_id, id, shipment_id, type, hazardous, container_type_code,
                 container_ownership, operation_id, ordinal)
                VALUES (%s, %s, %s, %s, false, %s, %s, %s, %s)''',
                (organization, str(uuid4()), sid, kind, code, owner, oid, 1))
    response = await api.post('/api/v1/transportations/container-fact', {
        'shipment_id': shipment, 'pending_operation_ids': [operation]}, roles=['shipments.read','organizations.read_employees','counterparties.read'])
    groups = {(row.get('container_type_code'), row.get('container_ownership')): row['quantity'] for row in response['groups']}
    assert groups == {('40HC','COC'):2, ('20DC','SOC'):1, (None,None):1}
    assert response['operations'] == [{'operation_id':operation,'created_quantity':1}]
    incomplete = await api.post('/api/v1/transportations/list', {'shipment_id':shipment,'incomplete_container':True})
    assert len(incomplete['items']) == 1
    assert incomplete['items'][0]['type'] == 'container'


async def test_fact_requires_shipment_read(api):
    await api.post('/api/v1/transportations/container-fact', {'shipment_id':str(uuid4()),'pending_operation_ids':[]},roles=[],expected=403)

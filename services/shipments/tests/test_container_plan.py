"""План сопоставляется с актуальным фактом, а не с историей создания перевозок."""
from uuid import uuid4
import pytest
from test_shipments import setup_item


async def plan(api, shipment):
    return await api.post('/api/v1/shipment-items/list', {'shipment_id': shipment['id']})


async def test_manual_fact_and_container_changes_are_counted(api, container_facts):
    shipment, item = await setup_item(api, 2)
    container_facts['groups'] = [
        {'container_type_code': '40HC', 'container_ownership': 'COC', 'quantity': 3},
        {'container_type_code': '20DC', 'container_ownership': 'SOC', 'quantity': 1},
        {'container_type_code': '40HC', 'quantity': 2},
    ]
    result = await plan(api, shipment)
    position = result['items'][0]
    assert (position['created_quantity'], position['available_quantity'], position['excess_quantity']) == (3, 0, 1)
    assert result['unplanned_items'] == container_facts['groups'][1:]
    # Изменение SOC/COC у фактической перевозки сразу меняет сопоставление.
    container_facts['groups'][0]['container_ownership'] = 'SOC'
    result = await plan(api, shipment)
    assert result['items'][0]['created_quantity'] == 0
    assert result['items'][0]['available_quantity'] == 2
    assert len(result['unplanned_items']) == 3


async def test_pending_creation_is_not_counted_twice(api, container_facts):
    shipment, item = await setup_item(api, 5)
    created = await api.post('/api/v1/transportation-operations/create', {
        'item_id': item['id'], 'expected_revision': item['revision'], 'quantity': 3,
        'idempotency_key': str(uuid4()),
    }, expected=202)
    operation = created['operation']['id']
    container_facts['groups'] = [{'container_type_code': '40HC', 'container_ownership': 'COC', 'quantity': 2}]
    container_facts['operations'] = [{'operation_id': operation, 'created_quantity': 2}]
    row = (await plan(api, shipment))['items'][0]
    assert (row['created_quantity'], row['pending_quantity'], row['available_quantity']) == (2, 1, 2)
    container_facts['groups'][0]['quantity'] = 3
    container_facts['operations'][0]['created_quantity'] = 3
    row = (await plan(api, shipment))['items'][0]
    assert (row['created_quantity'], row['pending_quantity'], row['available_quantity']) == (3, 0, 2)


async def test_plan_can_be_reduced_below_fact_and_to_zero(api, container_facts):
    shipment, item = await setup_item(api, 5)
    container_facts['groups'] = [{'container_type_code': '40HC', 'container_ownership': 'COC', 'quantity': 4}]
    for revision, quantity in [(1, 2), (2, 0)]:
        await api.post('/api/v1/shipment-items/update', {
            'item_id': item['id'], 'expected_revision': revision, 'planned_quantity': quantity,
        })
        row = (await plan(api, shipment))['items'][0]
        assert row['excess_quantity'] == 4 - quantity
        assert row['available_quantity'] == 0


async def test_identical_plan_rows_merge_with_idempotency(api):
    shipment, item = await setup_item(api, 5)
    selected = (await api.post('/api/v1/shipments/get', {'shipment_id': shipment['id']}))['shipment']
    body = {'shipment_id': shipment['id'], 'expected_revision': selected['revision'],
        'container_type_code': '40HC', 'container_ownership': 'COC', 'planned_quantity': 2,
        'idempotency_key': str(uuid4())}
    first = await api.post('/api/v1/shipment-items/add', body, expected=201)
    assert await api.post('/api/v1/shipment-items/add', body, expected=201) == first
    rows = (await plan(api, shipment))['items']
    assert len(rows) == 1
    assert rows[0]['id'] == item['id']
    assert rows[0]['planned_quantity'] == 7
    assert rows[0]['revision'] == 2


async def test_unavailable_fact_does_not_look_like_zero(api, mockserver):
    shipment, _ = await setup_item(api)
    @mockserver.json_handler('/api/v1/transportations/container-fact')
    def unavailable(request):
        return mockserver.make_response(status=503, json={'error':'unavailable','message':'Retry'})
    result = await api.post('/api/v1/shipment-items/list', {'shipment_id': shipment['id']}, expected=503)
    assert result['error'] == 'container_fact_unavailable'

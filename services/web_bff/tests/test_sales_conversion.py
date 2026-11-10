from uuid import uuid4

import pytest


def uid():
    return str(uuid4())


@pytest.mark.parametrize('with_purchase', [False, True])
async def test_proposal_preserves_prices_quote_source_and_history(
    browser, mockserver, with_purchase
):
    await browser.sign_in()
    sale = {
        'price_minor': 123456,
        'currency': 'USD',
        'price_mode': 'gross',
        'vat_mode': 'rate',
        'vat_rate_basis_points': 2000,
    }
    line = {
        'id': uid(), 'label_id': uid(), 'unit': 'container',
        'quantity_milli': 2500, 'sale': sale,
    }
    if with_purchase:
        line.update(
            supplier_id=uid(), route_segment_id=uid(),
            purchase={'price_minor': 80000, 'currency': 'EUR',
                      'price_mode': 'net', 'vat_mode': 'exempt'},
            quote_source={'rfq_id': uid(), 'revision': 3,
                          'quote_id': uid(), 'line_id': uid()},
        )
    body = {
        'id': uid(), 'inquiry_id': uid(), 'expected_revision': 2,
        'title': 'Мультимодальная перевозка', 'calculation_date': '2026-11-10',
        'valid_until': '2026-11-20', 'delivery_days': 35,
        'note': 'Согласовать сроки', 'lines': [line],
    }
    amounts = {'net_minor': 100000, 'vat_minor': 20000, 'total_minor': 120000}
    zero = {'net_minor': 0, 'vat_minor': 0, 'total_minor': 0}
    total = {'currency': 'USD', 'sale': amounts, 'purchase': zero}
    proposal = {
        'input': {k: v for k, v in body.items() if k != 'expected_revision'},
        'revision': 3, 'status': 'offered', 'reporting_currency': 'RUB',
        'rates': [{'currency': 'USD', 'rate_millionths': 81000000}],
        'lines': [{'input': line, 'label_name': 'Морской фрахт', 'sale': amounts}],
        'totals': [total], 'reporting': total,
        'customer_name': 'Клиент', 'inquiry_revision': 7,
    }
    if with_purchase:
        proposal['lines'][0].update(supplier_name='Перевозчик', purchase=amounts)

    @mockserver.json_handler('/api/v1/sales/proposals/save')
    def save(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json == body
        return {'item': proposal}

    history = {'snapshot': proposal, 'created_at': '2026-11-10T12:00:00+00:00',
               'actor': 'https://issuer.example#author'}

    @mockserver.json_handler('/api/v1/sales/proposals/history')
    def history_endpoint(request):
        assert request.json == {'id': body['id'], 'after_revision': 2}
        return {'items': [history]}

    response = await browser.post('/api/v1/sales/proposals/save', body)
    assert response.status == 200
    assert response.json() == {'item': proposal}
    response = await browser.post('/api/v1/sales/proposals/history', {
        'id': body['id'], 'after_revision': 2,
    })
    assert response.status == 200
    assert response.json() == {'items': [history]}


async def test_rfq_preserves_supplier_variants_with_multiple_quote_lines(browser, mockserver):
    await browser.sign_in()
    segment = {'id': uid(), 'type': 'air', 'air': {
        'origin_airport': {'id': uid(), 'kind': 'airport', 'name': 'Origin',
                           'country_code': 'CN', 'code': 'PVG'},
        'eta': '2026-11-18', 'etd': '2026-11-12', 'flight_number': 'TEST-123',
    }}
    quote = {
        'id': uid(), 'title': 'Экспресс', 'supplier_id': uid(),
        'supplier_name': 'Поставщик', 'valid_until': '2026-11-20',
        'delivery_days': 6, 'note': 'Из переписки',
        'lines': [
            {'id': uid(), 'label_id': uid(), 'label_name': 'Фрахт',
             'unit': 'ton', 'quantity_milli': 1250,
             'price': {'price_minor': 90000, 'currency': 'USD',
                       'price_mode': 'net', 'vat_mode': 'exempt'}},
            {'id': uid(), 'label_id': uid(), 'unit': 'piece', 'quantity_milli': 1000,
             'price': {'price_minor': 10000, 'currency': 'RUB',
                       'price_mode': 'gross', 'vat_mode': 'rate',
                       'vat_rate_basis_points': 1000}},
        ],
    }
    body = {
        'id': uid(), 'inquiry_id': uid(), 'route_segment_id': segment['id'],
        'title': 'Авиаперевозка', 'responsible_subject': 'author',
        'answer_by': '2026-11-11', 'note': 'Срочный запрос',
        'status': 'ready', 'quotes': [quote], 'expected_revision': 1,
    }
    item = {'input': {k: v for k, v in body.items() if k != 'expected_revision'},
            'revision': 2, 'inquiry_number': 'ENQ-42', 'route_segment': segment}

    @mockserver.json_handler('/api/v1/sales/rfqs/save')
    def save(request):
        assert request.json == body
        return {'item': item}

    response = await browser.post('/api/v1/sales/rfqs/save', body)
    assert response.status == 200
    assert response.json() == {'item': item}


async def test_inquiry_preserves_multimodal_route_and_conversion_state(browser, mockserver):
    await browser.sign_in()
    body = {
        'id': uid(), 'customer_id': uid(), 'manager_subject': 'author',
        'status': 'agreed', 'expected_revision': 4,
        'cargo_description': 'Оборудование', 'incoterms': 'FOB',
        'delivery_city': 'Москва', 'delivery_country': 'RU',
        'ready_date': '2026-11-11', 'desired_delivery_date': '2026-12-01',
        'route_segments': [
            {'id': uid(), 'type': 'sea', 'sea': {
                'pol': {'id': uid(), 'kind': 'sea_port', 'name': 'Shanghai',
                        'country_code': 'CN', 'code': 'CNSHA'},
                'pod': {'id': uid(), 'kind': 'sea_port', 'name': 'Vladivostok',
                        'country_code': 'RU'},
                'eta': '2026-11-20', 'departure_date': '2026-11-12',
                'vessel': 'Test vessel', 'voyage': '123',
            }},
            {'id': uid(), 'type': 'air', 'air': {
                'eta': '2026-11-23', 'etd': '2026-11-22',
                'departure_date': '2026-11-22', 'arrival_date': '2026-11-23',
                'flight_number': 'TEST-456',
            }},
        ],
    }
    item = {k: v for k, v in body.items() if k != 'expected_revision'}
    item.update(organization_id=uid(), number='ENQ-43', revision=5, converting=True,
                accepted_proposal_id=uid(), accepted_proposal_revision=2)

    @mockserver.json_handler('/api/v1/sales/inquiries/save')
    def save(request):
        assert request.json == body
        return {'item': item}

    response = await browser.post('/api/v1/sales/inquiries/save', body)
    assert response.status == 200
    assert response.json() == {'item': item}

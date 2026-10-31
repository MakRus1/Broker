import { describe, expect, it } from 'vitest';
import { CalendarDate } from '../../shared/dates/CalendarDate';
import { shipmentRegistryRequest, initialShipmentQuery } from './shipmentRegistry';
describe('фильтр дат поставок', () => {
  it('сериализует календарные даты без времени и часового пояса', () => {
    const date = CalendarDate.fromISO('2026-01-15');
    const request = shipmentRegistryRequest({
      ...initialShipmentQuery,
      filters: [{ field: 'shipment_date', date_from: date, date_to: date }],
    });
    expect(JSON.parse(JSON.stringify(request))).toMatchObject({
      filters: [{ field: 'shipment_date', date_from: '2026-01-15', date_to: '2026-01-15' }],
    });
    expect(date.toDisplayString()).toBe('15.01.2026');
  });
  it('продолжает страницу по переданному курсору', () => {
    const cursor = { id: 'id', scope: 'scope', date_value: '2026-01-15' };
    expect(shipmentRegistryRequest(initialShipmentQuery, cursor).cursor).toBe(cursor);
    expect(shipmentRegistryRequest(initialShipmentQuery)).not.toHaveProperty('cursor');
  });
});

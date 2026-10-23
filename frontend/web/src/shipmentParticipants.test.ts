import { expect, it } from 'vitest';
import { changeShipmentCustomer } from './shipmentParticipants';

it('подставляет клиента в получателя и обновляет автоматически подставленное значение', () => {
  const first = changeShipmentCustomer({ customer_id: '', recipient_id: '' }, 'client-a');
  expect(first).toEqual({ customer_id: 'client-a', recipient_id: 'client-a' });
  expect(changeShipmentCustomer(first, 'client-b').recipient_id).toBe('client-b');
  expect(changeShipmentCustomer(first, '').recipient_id).toBe('');
});

it.each(['recipient-c', ''])('сохраняет ручной выбор получателя: %s', (recipient) => {
  expect(
    changeShipmentCustomer({ customer_id: 'client-a', recipient_id: recipient }, 'client-b'),
  ).toEqual({ customer_id: 'client-b', recipient_id: recipient });
});

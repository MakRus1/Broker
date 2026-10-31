import { describe, expect, it } from 'vitest';
import {
  counterpartyChanges,
  historyFieldValue,
  type CounterpartyEvent,
} from './counterpartyChanges';

const event: CounterpartyEvent = {
  id: 1,
  action: 'update_counterparty',
  actor: 'issuer#subject',
  occurred_at: '2026-10-21T12:00:00Z',
  before: {
    name: 'Клиент',
    legal_name: 'ООО Клиент',
    roles: ['customer', 'carrier'],
    contacts: [],
    archived: false,
  },
  after: {
    name: 'Клиент',
    legal_name: null,
    roles: ['carrier', 'customer'],
    contacts: [{ name: 'Анна', position: null, email: null, phone: null }],
    archived: true,
  },
};
describe('Сравнение истории контрагентов', () => {
  it('показывает очистку реквизита, добавление контакта и архивирование', () => {
    expect(
      counterpartyChanges(event)
        .filter((field) => field.changed)
        .map((field) => field.key),
    ).toEqual(['legal_name', 'contacts', 'archived']);
  });
  it('различает неизвестное значение в старом аудите и явный null', () => {
    expect(historyFieldValue({ name: 'Старый клиент' }, 'legal_name')).toBe('Не сохранено');
    expect(historyFieldValue({ name: 'Клиент', legal_name: null }, 'legal_name')).toBe(
      'Не указано',
    );
  });
  it('показывает роли и контакты без JSON и технических значений', () => {
    expect(historyFieldValue(event.after, 'roles')).toBe('Перевозчик, Клиент');
    expect(historyFieldValue(event.after, 'contacts')).toBe('Анна');
    expect(historyFieldValue(event.after, 'archived')).toBe('В архиве');
  });
  it('для создания отмечает только сохранённые поля', () => {
    expect(
      counterpartyChanges({ ...event, before: undefined, after: { name: 'Клиент' } })
        .filter((field) => field.changed)
        .map((field) => field.key),
    ).toEqual(['name']);
  });
});

import { describe, expect, it } from 'vitest';
import { isInn, registryRequisites } from './registry';
import { counterpartyDetails, restoreCounterpartyDraft } from './counterparties';

describe('ИНН', () => {
  it.each(['7707083893', '7719402047', '784806113663'])('принимает %s', (value) =>
    expect(isInn(value)).toBe(true),
  );
  it.each(['7707083894', '0000000000', '123', '770708389a', '123456789012'])(
    'отклоняет %s',
    (value) => expect(isInn(value)).toBe(false),
  );
});
it('подставляет только реквизиты, не меняя роли и контакты', () => {
  const draft = {
    ...counterpartyDetails(),
    roles: ['customer'],
    contacts: [{ name: 'Анна', position: '', email: '', phone: '' }],
  };
  const values = registryRequisites({
    name: 'Компания',
    legal_name: null,
    tax_id: '7707083893',
    kpp: null,
    registration_id: null,
    legal_address: 'Москва',
    status: 'ACTIVE',
  });
  expect({ ...draft, ...values }).toEqual({
    ...draft,
    name: 'Компания',
    legal_name: '',
    country_code: 'RU',
    tax_id: '7707083893',
    kpp: '',
    registration_id: '',
    legal_address: 'Москва',
  });
});
it('восстанавливает черновик, сохранённый до добавления КПП', () => {
  const fallback = { id: 'id', baseRevision: 0, value: counterpartyDetails() };
  const { kpp, ...old } = fallback.value;
  expect(
    restoreCounterpartyDraft(
      JSON.stringify({ ...fallback, value: { ...old, name: 'Черновик' } }),
      fallback,
    ).value.kpp,
  ).toBe('');
});

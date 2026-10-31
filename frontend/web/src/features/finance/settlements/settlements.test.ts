import { describe, expect, it } from 'vitest';
import { settlementScope } from './settlements';
import { scopedFilters } from '../../registry/registryFilters';

const filters = { own_company_id: 'own', contract_id: 'contract', currency: 'USD' };
describe('область взаиморасчётов', () => {
  it('долг ограничен контрагентом, юрлицом, договором, валютой и действующими неоплаченными счетами', () => {
    const scope = settlementScope('party', filters, 'outgoing', true);
    expect(scope.filters).toEqual([
      { field: 'counterparty_id', values: ['party'] },
      { field: 'archived', values: ['false'] },
      { field: 'own_company_id', values: ['own'] },
      { field: 'currency', values: ['USD'] },
      { field: 'payment_status', values: ['unpaid', 'partial'] },
      { field: 'contract_id', values: ['contract'] },
      { field: 'overdue', values: ['true'] },
    ]);
  });
  it('платежи не фильтруются по договору или просрочке', () => {
    const scope = settlementScope('party', filters, 'receipt', true);
    expect(scope.filters.map((field) => field.field)).toEqual([
      'counterparty_id',
      'archived',
      'own_company_id',
      'currency',
      'has_unallocated',
    ]);
  });
  it('локальные фильтры не снимают область вложенной таблицы', () => {
    const scope = settlementScope('party', filters, 'outgoing', false);
    const values = scopedFilters(
      [
        { field: 'counterparty_id', values: ['other'] },
        { field: 'number', query: 'ИС-' },
      ],
      scope,
    );
    expect(values.filter((field) => field.field === 'counterparty_id')).toEqual([
      { field: 'counterparty_id', values: ['party'] },
    ]);
    expect(values).toContainEqual({ field: 'number', query: 'ИС-' });
    expect(scopedFilters([], scope)).toEqual(scope.filters);
  });
});

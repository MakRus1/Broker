import { describe, expect, it } from 'vitest';
import { reconciliationCsv, reconciliationLink, type Reconciliation } from './reconciliation';

describe('Акт сверки', () => {
  it('экспортирует точные суммы и защищает текст от формул CSV', () => {
    const data: Reconciliation = {
      totals: [
        {
          currency: 'RUB',
          opening_minor: '9007199254740993',
          debit_minor: '0',
          credit_minor: '0',
          closing_minor: '9007199254740993',
        },
      ],
      operations: [],
    };
    const csv = reconciliationCsv(data, {
      counterparty: '=HYPERLINK("url")',
      ownCompany: 'Компания',
      contract: 'Все',
      from: '01.01.2026',
      to: '31.12.2026',
    });
    expect(csv).toContain("'=HYPERLINK");
    expect(csv.replaceAll('\u00a0', ' ')).toContain('90 071 992 547 409,93');
    expect(csv.startsWith('\ufeff')).toBe(true);
  });
  it('открывает документы в соответствующих реестрах', () => {
    expect(
      reconciliationLink({
        kind: 'receipt',
        id: '123',
        date: '2026-10-31',
        number: 'П-1',
        own_company_name: 'Компания',
        currency: 'RUB',
        debit_minor: '0',
        credit_minor: '100',
        balance_minor: '-100',
      }),
    ).toBe('#/receipts/123');
    expect(
      reconciliationLink({
        kind: 'incoming_invoice',
        id: '456',
        date: '2026-10-31',
        number: 'С-1',
        own_company_name: 'Компания',
        currency: 'RUB',
        debit_minor: '0',
        credit_minor: '100',
        balance_minor: '-100',
      }),
    ).toBe('#/incoming-invoices/456');
  });
});

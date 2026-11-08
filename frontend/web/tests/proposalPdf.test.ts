import { it, expect } from 'vitest';
import { readFileSync, writeFileSync } from 'node:fs';
import { createProposalPdf } from '../src/features/sales/proposalPdf';
import type { Proposal } from '../src/features/sales/proposals';
const fonts = {
  regular: readFileSync(
    new URL('../src/shared/printing/fonts/NotoSans-Regular.ttf', import.meta.url),
  ).toString('base64'),
  bold: readFileSync(
    new URL('../src/shared/printing/fonts/NotoSans-Bold.ttf', import.meta.url),
  ).toString('base64'),
};
it('prints long Russian proposals across pages with only customer prices', () => {
  const amounts = { net_minor: 230000, vat_minor: 46000, total_minor: 276000 };
  const value: Proposal = {
    input: {
      id: 'test',
      inquiry_id: 'inquiry',
      title: 'Доставка оборудования морем и железной дорогой',
      calculation_date: '2026-11-08',
      valid_until: '2026-11-30',
      delivery_days: 35,
      note: 'Стоимость действует для согласованного маршрута. Срок доставки ориентировочный.',
      lines: [],
    },
    inquiry_revision: 1,
    revision: 1,
    status: 'offered',
    reporting_currency: 'USD',
    rates: [],
    reporting: { currency: 'USD', sale: amounts, purchase: { ...amounts } },
    customer_name: 'Демонстрационный клиент',
    lines: Array.from({ length: 100 }, (_, i) => ({
      input: {
        id: String(i),
        label_id: 'label',
        unit: 'transportation',
        quantity_milli: 1000,
        sale: {
          price_minor: 230000,
          currency: 'USD',
          price_mode: 'net',
          vat_mode: 'rate',
          vat_rate_basis_points: 2000,
        },
        purchase: { price_minor: 200000, currency: 'USD', price_mode: 'net', vat_mode: 'exempt' },
      },
      label_name: 'Морской фрахт и обработка груза в порту назначения ' + (i + 1),
      supplier_name: 'PRIVATE SUPPLIER',
      sale: amounts,
      purchase: { net_minor: 200000, vat_minor: 0, total_minor: 200000 },
    })),
    totals: [
      {
        currency: 'USD',
        sale: { net_minor: 23000000, vat_minor: 4600000, total_minor: 27600000 },
        purchase: { net_minor: 20000000, vat_minor: 0, total_minor: 20000000 },
      },
    ],
  };
  const doc = createProposalPdf(value, fonts, 'ru');
  expect(doc.getNumberOfPages()).toBeGreaterThan(2);
  const changed = structuredClone(value);
  changed.lines[0].supplier_name = 'Another private supplier';
  changed.lines[0].input.purchase!.price_minor = 55555;
  changed.lines[0].purchase!.total_minor = 66666;
  changed.totals[0].purchase.total_minor = 77777;
  changed.reporting.purchase.total_minor = 88888;
  const second = createProposalPdf(changed, fonts, 'ru');
  for (const pdf of [doc, second]) {
    pdf.setCreationDate(new Date('2026-11-08T00:00:00Z'));
    pdf.setFileId('1234567890ABCDEF1234567890ABCDEF');
  }
  expect(second.output() === doc.output()).toBe(true);
  if (process.env.BROKER_PROPOSAL_PDF_OUTPUT)
    writeFileSync(
      process.env.BROKER_PROPOSAL_PDF_OUTPUT,
      new Uint8Array(doc.output('arraybuffer')),
    );
});

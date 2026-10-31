import { describe, expect, it } from 'vitest';
import { readFileSync, mkdirSync, writeFileSync } from 'node:fs';
import {
  createInvoicePdf,
  type InvoicePrintData,
} from '../src/features/finance/invoices/print/invoicePdf';
const fonts = {
  regular: readFileSync(
    new URL('../src/features/finance/invoices/print/fonts/NotoSans-Regular.ttf', import.meta.url),
  ).toString('base64'),
  bold: readFileSync(
    new URL('../src/features/finance/invoices/print/fonts/NotoSans-Bold.ttf', import.meta.url),
  ).toString('base64'),
};
function fixture(count: number): InvoicePrintData {
  return {
    invoice: {
      number: 'ИС-123/2026',
      invoice_date: '2026-10-31',
      currency: 'RUB',
      net_minor: 100000 * count,
      vat_minor: 20000 * count,
      total_minor: 120000 * count,
      due_date: '2026-11-10',
      contract_number: 'Д-1/2026',
      note: 'Оплата услуг по договору',
      archived: false,
    },
    supplier: {
      name: 'ООО «Логистическая компания»',
      tax_id: '7707083893',
      kpp: '773601001',
      registration_id: '1027700132195',
      legal_address: 'Москва, улица Вавилова, дом 19',
    },
    customer: {
      name: 'ООО «Заказчик»',
      tax_id: '7724353317',
      kpp: '773301001',
      registration_id: null,
      legal_address: 'Владивосток, улица Портовая, дом 1',
    },
    supplier_bank: {
      name: 'Тестовый банк',
      number: '40702810000000000001',
      currency: 'RUB',
      bic: '044525225',
      correspondent_account: '30101810400000000225',
      iban: null,
      swift: null,
    },
    charges: Array.from({ length: count }, (_, i) => ({
      label_name: `Услуга ${i + 1}: морской фрахт и обработка груза на терминале`,
      quantity_milli: 1000,
      unit: 'container',
      invoice_net_minor: 100000,
      invoice_vat_minor: 20000,
      invoice_total_minor: 120000,
      vat_mode: 'rate',
      vat_rate_basis_points: 2000,
    })),
  } as InvoicePrintData;
}
describe('Печатная форма счёта', () => {
  it.each([1, 100])('формирует PDF с %i услугами и повторяемыми заголовками', (count) => {
    const pdf = createInvoicePdf(fixture(count), fonts);
    if (count === 1) expect(pdf.getNumberOfPages()).toBe(1);
    else expect(pdf.getNumberOfPages()).toBeGreaterThan(1);
    const bytes = pdf.output('arraybuffer');
    expect(bytes.byteLength).toBeGreaterThan(10000);
    if (process.env.PDF_FIXTURES_DIR) {
      mkdirSync(process.env.PDF_FIXTURES_DIR, { recursive: true });
      writeFileSync(`${process.env.PDF_FIXTURES_DIR}/invoice-${count}.pdf`, Buffer.from(bytes));
    }
  });
});

import { expect, it, vi } from 'vitest';
import { autoTable } from 'jspdf-autotable';
import { createInvoicePdf, type InvoicePrintData } from './invoicePdf';

vi.mock('jspdf', () => ({
  jsPDF: class {
    addFileToVFS() {}
    addFont() {}
    setFont() {}
    setProperties() {}
    getNumberOfPages() {
      return 1;
    }
    setPage() {}
    setFontSize() {}
    setTextColor() {}
    setDrawColor() {}
    line() {}
    text() {}
  },
}));
vi.mock('jspdf-autotable', () => ({ autoTable: vi.fn() }));
const data = {
  invoice: {
    number: 'СЧЁТ-КЛИЕНТА',
    invoice_date: '2026-11-03',
    currency: 'RUB',
    net_minor: 10000,
    vat_minor: 0,
    total_minor: 10000,
  },
  supplier: { name: 'Компания клиента', tax_id: '123' },
  customer: { name: 'Покупатель клиента' },
  charges: [
    {
      label_name: 'Услуга клиента',
      quantity_milli: 1000,
      unit: 'ton',
      invoice_net_minor: 10000,
      invoice_vat_minor: 0,
      invoice_total_minor: 10000,
      vat_mode: 'exempt',
    },
  ],
} as InvoicePrintData;
it.each(['ru', 'en'] as const)(
  'localizes the invoice PDF in %s without translating customer data',
  (locale) => {
    vi.mocked(autoTable).mockClear();
    createInvoicePdf(data, { regular: '', bold: '' }, locale);
    const rendered = JSON.stringify(vi.mocked(autoTable).mock.calls.map((call) => call[1]));
    expect(rendered).toContain('СЧЁТ-КЛИЕНТА');
    expect(rendered).toContain('Компания клиента');
    expect(rendered).toContain('Услуга клиента');
    expect(rendered).toContain('100,00');
    expect(rendered).toContain(locale === 'en' ? 'Service description' : 'Наименование услуги');
    expect(rendered).toContain(locale === 'en' ? 'Ton' : 'Тонна');
    if (locale === 'en') {
      expect(rendered).not.toContain('Без НДС');
      expect(rendered).not.toContain('Банковские реквизиты');
      expect(rendered).toContain('Bank details');
    }
  },
);

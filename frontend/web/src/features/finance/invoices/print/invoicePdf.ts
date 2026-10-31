import { jsPDF } from 'jspdf';
import { autoTable } from 'jspdf-autotable';
import type { Invoice } from '../invoices';
import { formatFixed, type ServiceCharge } from '../../services/serviceCharges';
import { financialLabelUnits, vatLabel } from '../../labels/financialLabels';
import { CalendarDate } from '../../../../shared/dates/CalendarDate';

export interface PrintParty {
  name: string;
  tax_id: string | null;
  kpp: string | null;
  registration_id: string | null;
  legal_address: string | null;
}
export interface PrintBank {
  name: string;
  number: string;
  currency: string;
  bic: string | null;
  swift: string | null;
  correspondent_account: string | null;
  iban: string | null;
}
export interface InvoicePrintData {
  invoice: Invoice;
  charges: ServiceCharge[];
  supplier: PrintParty;
  customer: PrintParty;
  supplier_bank?: PrintBank;
}
export type PdfFonts = { regular: string; bold: string };
const text = (value: string) => value.replace(/[\u0000-\u0009\u000b-\u001f\u007f]/g, ' ');
const date = (value: string) => CalendarDate.fromISO(value).toDisplayString();
const party = (value: PrintParty) =>
  [
    value.name,
    value.tax_id && `ИНН ${value.tax_id}`,
    value.kpp && `КПП ${value.kpp}`,
    value.registration_id && `Рег. № ${value.registration_id}`,
    value.legal_address,
  ]
    .filter(Boolean)
    .join('\n');

/** Суммы берём из снимка Finance, не пересчитываем округлённую цену услуги. */
export function createInvoicePdf(data: InvoicePrintData, fonts: PdfFonts): jsPDF {
  const doc = new jsPDF({ unit: 'mm', format: 'a4', compress: true, putOnlyUsedFonts: true });
  doc.addFileToVFS('NotoSans-Regular.ttf', fonts.regular);
  doc.addFont('NotoSans-Regular.ttf', 'NotoSans', 'normal');
  doc.addFileToVFS('NotoSans-Bold.ttf', fonts.bold);
  doc.addFont('NotoSans-Bold.ttf', 'NotoSans', 'bold');
  doc.setFont('NotoSans');
  doc.setProperties({ title: `Счёт № ${text(data.invoice.number)}`, creator: 'Broker' });
  let y = 16;
  const table = (options: Parameters<typeof autoTable>[1]) => {
    autoTable(doc, {
      startY: y,
      margin: { top: 16, bottom: 22, left: 16, right: 16 },
      styles: {
        font: 'NotoSans',
        fontSize: 9,
        cellPadding: 2.5,
        overflow: 'linebreak',
        textColor: [30, 42, 60],
      },
      headStyles: { fillColor: [235, 240, 248], textColor: [30, 42, 60], fontStyle: 'bold' },
      theme: 'plain',
      rowPageBreak: 'avoid',
      ...options,
      didDrawPage: (hook) => {
        y = hook.cursor?.y ?? y;
      },
    });
    y += 5;
  };
  table({
    body: [
      [`Счёт на оплату № ${text(data.invoice.number)}\nот ${date(data.invoice.invoice_date)}`],
    ],
    styles: {
      font: 'NotoSans',
      fontStyle: 'bold',
      fontSize: 17,
      cellPadding: 0,
      textColor: [30, 42, 60],
    },
  });
  if (data.invoice.archived) table({ body: [['Архивный счёт']] });
  const bank = data.supplier_bank;
  table({
    body: bank
      ? [
          ['Банк получателя', text(bank.name)],
          [
            'Реквизиты банка',
            [
              bank.bic && `БИК ${bank.bic}`,
              bank.swift && `SWIFT ${bank.swift}`,
              bank.correspondent_account && `Корр. счёт ${bank.correspondent_account}`,
            ]
              .filter(Boolean)
              .join('\n') || '—',
          ],
          [
            'Счёт получателя',
            [bank.number, bank.iban && `IBAN ${bank.iban}`].filter(Boolean).join('\n'),
          ],
        ]
      : [['Банковские реквизиты', 'Не указаны']],
    columnStyles: { 0: { cellWidth: 42, fontStyle: 'bold' } },
    alternateRowStyles: { fillColor: [246, 248, 251] },
  });
  table({
    body: [
      ['Поставщик', text(party(data.supplier))],
      ['Покупатель', text(party(data.customer))],
      ...(data.invoice.contract_number ? [['Договор', text(data.invoice.contract_number)]] : []),
      ...(data.invoice.due_date ? [['Оплатить до', date(data.invoice.due_date)]] : []),
    ],
    columnStyles: { 0: { cellWidth: 30, fontStyle: 'bold' } },
  });
  table({
    head: [
      [
        '№',
        'Наименование услуги',
        'Кол-во / ед.',
        `Без НДС\n${data.invoice.currency}`,
        'НДС',
        `С НДС\n${data.invoice.currency}`,
      ],
    ],
    body: data.charges.map((charge, index) => [
      String(index + 1),
      text(charge.label_name),
      `${formatFixed(charge.quantity_milli, 3)}\n${financialLabelUnits[charge.unit]}`,
      formatFixed(charge.invoice_net_minor, 2),
      `${formatFixed(charge.invoice_vat_minor, 2)}\n${vatLabel(charge)}`,
      formatFixed(charge.invoice_total_minor, 2),
    ]),
    showHead: 'everyPage',
    theme: 'striped',
    columnStyles: {
      0: { cellWidth: 12 },
      1: { cellWidth: 54 },
      2: { cellWidth: 28 },
      3: { cellWidth: 28, halign: 'right' },
      4: { cellWidth: 24, halign: 'right' },
      5: { cellWidth: 32, halign: 'right' },
    },
  });
  table({
    body: [
      ['Всего без НДС', `${formatFixed(data.invoice.net_minor, 2)} ${data.invoice.currency}`],
      ['НДС', `${formatFixed(data.invoice.vat_minor, 2)} ${data.invoice.currency}`],
      ['Итого к оплате', `${formatFixed(data.invoice.total_minor, 2)} ${data.invoice.currency}`],
    ],
    pageBreak: 'avoid',
    styles: { font: 'NotoSans', fontSize: 11, cellPadding: 2.5, fontStyle: 'bold' },
    columnStyles: { 1: { halign: 'right' } },
  });
  if (!data.charges.length) table({ body: [['Услуги пока не добавлены.']] });
  if (data.invoice.note)
    table({
      body: [['Примечание', text(data.invoice.note)]],
      columnStyles: { 0: { cellWidth: 30 } },
    });
  const pages = doc.getNumberOfPages();
  for (let page = 1; page <= pages; page++) {
    doc.setPage(page);
    doc.setFont('NotoSans', 'normal');
    doc.setFontSize(8);
    doc.setTextColor(100);
    doc.setDrawColor(220);
    doc.line(16, 280, 194, 280);
    doc.text('Broker · Счёт на оплату', 16, 286);
    doc.text(`Страница ${page} из ${pages}`, 194, 286, { align: 'right' });
  }
  return doc;
}

import { jsPDF } from 'jspdf';
import { autoTable } from 'jspdf-autotable';
import { loadPdfFonts } from '../../shared/printing/pdfFonts';
import { translate, type Locale } from '../../../../shared/i18n/catalog';
import { formatFixed } from '../finance/services/serviceCharges';
import type { Proposal } from './proposals';
// Only the sale side is included: purchasing rates, suppliers and margin never enter the PDF.
export function createProposalPdf(
  value: Proposal,
  fonts: { regular: string; bold: string },
  locale: Locale,
) {
  const translateMessage = (key: Parameters<typeof translate>[1]) => translate(locale, key);
  const doc = new jsPDF({ unit: 'mm', format: 'a4', compress: true, putOnlyUsedFonts: true });
  doc.addFileToVFS('NotoSans-Regular.ttf', fonts.regular);
  doc.addFont('NotoSans-Regular.ttf', 'NotoSans', 'normal');
  doc.addFileToVFS('NotoSans-Bold.ttf', fonts.bold);
  doc.addFont('NotoSans-Bold.ttf', 'NotoSans', 'bold');
  doc.setProperties({ title: value.input.title, creator: 'Broker' });
  const date = new Intl.DateTimeFormat(locale, { dateStyle: 'medium', timeZone: 'UTC' });
  const formatDate = (iso: string) => date.format(new Date(iso + 'T00:00:00Z'));
  const margin = { top: 16, bottom: 22, left: 16, right: 16 };
  const ink: [number, number, number] = [30, 42, 60];
  const muted: [number, number, number] = [87, 100, 117];
  const tint: [number, number, number] = [239, 243, 248];
  let y = margin.top;
  const table = (options: Parameters<typeof autoTable>[1]) => {
    autoTable(doc, {
      startY: y,
      margin,
      theme: 'plain',
      rowPageBreak: 'avoid',
      styles: { font: 'NotoSans', fontSize: 9, cellPadding: 2.5, textColor: ink },
      headStyles: { fillColor: tint, fontStyle: 'bold' },
      ...options,
      didDrawPage: ({ cursor }) => {
        y = cursor?.y ?? y;
      },
    });
    y += 6;
  };

  table({
    body: [
      [
        {
          content: translateMessage('proposal.customerOffer'),
          styles: { fontSize: 10, textColor: muted },
        },
      ],
      [{ content: value.input.title, styles: { fontSize: 20, fontStyle: 'bold' } }],
    ],
    styles: { font: 'NotoSans', textColor: ink, cellPadding: { top: 0, bottom: 1.5 } },
  });
  table({
    body: [
      [translateMessage('sales.customer'), value.customer_name],
      [translateMessage('proposal.date'), formatDate(value.input.calculation_date)],
      ...(value.input.valid_until
        ? [[translateMessage('proposal.validUntil'), formatDate(value.input.valid_until)]]
        : []),
      ...(value.input.delivery_days !== undefined && value.input.delivery_days !== null
        ? [[translateMessage('proposal.deliveryDays'), String(value.input.delivery_days)]]
        : []),
      ...(value.input.note?.trim() ? [[translateMessage('sales.note'), value.input.note]] : []),
    ],
    styles: {
      font: 'NotoSans',
      fontSize: 10,
      cellPadding: { top: 1.3, bottom: 1.3 },
      textColor: ink,
    },
    columnStyles: { 0: { cellWidth: 49, textColor: muted } },
  });
  table({
    head: [
      [
        translateMessage('proposal.service'),
        translateMessage('proposal.quantity'),
        translateMessage('proposal.price'),
        translateMessage('proposal.net'),
        translateMessage('proposal.vat'),
        translateMessage('proposal.total'),
      ],
    ],
    body: value.lines.map((line) => {
      const { sale, unit, quantity_milli: quantity } = line.input;
      return [
        line.label_name + (line.input.note ? '\n' + line.input.note : ''),
        formatFixed(quantity, 3)
          .replace(/([,.]\d*?)0+$/, '$1')
          .replace(/[,.]$/, '') +
          '\n' +
          translateMessage(`registryValue.unit.${unit}`),
        `${formatFixed(sale.price_minor, 2)} ${sale.currency}\n${translateMessage(sale.price_mode === 'gross' ? 'proposal.priceGross' : 'proposal.priceNet')}`,
        formatFixed(line.sale.net_minor, 2),
        formatFixed(line.sale.vat_minor, 2),
        `${formatFixed(line.sale.total_minor, 2)} ${sale.currency}`,
      ];
    }),
    bodyStyles: { lineColor: [222, 228, 235], lineWidth: { bottom: 0.2 } },
    columnStyles: {
      0: { cellWidth: 47 },
      1: { cellWidth: 25 },
      2: { cellWidth: 30, halign: 'right' },
      3: { cellWidth: 25, halign: 'right' },
      4: { cellWidth: 20, halign: 'right' },
      5: { cellWidth: 31, halign: 'right', fontStyle: 'bold' },
    },
    didParseCell: ({ section, column, cell }) => {
      if (section === 'head' && column.index >= 2) cell.styles.halign = 'right';
    },
  });
  table({
    pageBreak: 'avoid',
    head: [
      [
        {
          content: translateMessage('proposal.totals'),
          colSpan: 4,
          styles: { fillColor: false, fontSize: 12, cellPadding: { bottom: 3 } },
        },
      ],
      [
        translateMessage('registryField.currency'),
        translateMessage('proposal.net'),
        translateMessage('proposal.vat'),
        { content: translateMessage('proposal.pdfTotal'), styles: { halign: 'right' } },
      ],
    ],
    body: value.totals.map((total) => [
      total.currency,
      formatFixed(total.sale.net_minor, 2),
      formatFixed(total.sale.vat_minor, 2),
      formatFixed(total.sale.total_minor, 2),
    ]),
    columnStyles: {
      0: { cellWidth: 49 },
      1: { halign: 'right' },
      2: { halign: 'right' },
      3: { halign: 'right', fontStyle: 'bold', fillColor: tint },
    },
    didParseCell: ({ section, row, column, cell }) => {
      if (section === 'head' && row.index === 1 && column.index > 0) cell.styles.halign = 'right';
    },
  });

  const pages = doc.getNumberOfPages();
  for (let page = 1; page <= pages; page++) {
    doc.setPage(page);
    doc.setFont('NotoSans', 'normal');
    doc.setFontSize(8);
    doc.setTextColor(...muted);
    doc.text(translate(locale, 'print.page', { page, pages }), 194, 286, { align: 'right' });
  }
  return doc;
}

export async function proposalPdf(value: Proposal, locale: Locale) {
  createProposalPdf(value, await loadPdfFonts(), locale).save(
    'proposal-' + value.input.id + '.pdf',
  );
}

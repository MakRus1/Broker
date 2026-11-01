import { jsPDF } from 'jspdf';
import { autoTable } from 'jspdf-autotable';
import { loadPdfFonts } from '../../../shared/printing/pdfFonts';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { formatFixed } from '../services/serviceCharges';
import {
  reconciliationLabels,
  type Reconciliation,
  type ReconciliationContext,
} from './reconciliation';
export async function downloadReconciliationPdf(
  data: Reconciliation,
  context: ReconciliationContext,
) {
  const fonts = await loadPdfFonts();
  const doc = new jsPDF({
    orientation: 'landscape',
    unit: 'mm',
    format: 'a4',
    compress: true,
    putOnlyUsedFonts: true,
  });
  doc.addFileToVFS('NotoSans-Regular.ttf', fonts.regular);
  doc.addFont('NotoSans-Regular.ttf', 'NotoSans', 'normal');
  doc.addFileToVFS('NotoSans-Bold.ttf', fonts.bold);
  doc.addFont('NotoSans-Bold.ttf', 'NotoSans', 'bold');
  doc.setFont('NotoSans');
  doc.setFontSize(16);
  doc.text('Акт сверки взаиморасчётов', 14, 18);
  doc.setFontSize(10);
  const intro = [
    `Контрагент: ${context.counterparty}`,
    `Наше юрлицо: ${context.ownCompany}`,
    `Договор: ${context.contract}`,
    `Период: ${context.from} — ${context.to}`,
    'Текущее состояние документов. Положительное сальдо — долг контрагента перед нами.',
  ];
  autoTable(doc, {
    startY: 24,
    body: intro.map((line) => [line]),
    theme: 'plain',
    styles: { font: 'NotoSans', fontSize: 9, cellPadding: 1.5 },
  });
  let y = (doc as jsPDF & { lastAutoTable: { finalY: number } }).lastAutoTable.finalY + 8;
  for (const total of data.totals) {
    const lines = data.operations.filter((item) => item.currency === total.currency);
    autoTable(doc, {
      startY: y,
      head: [[total.currency, 'Документ', 'Наше юрлицо', 'Дебет', 'Кредит', 'Сальдо']],
      body: [
        ['', 'Начальное сальдо', '', '', '', formatFixed(total.opening_minor, 2)],
        ...lines.map((item) => [
          CalendarDate.fromISO(item.date).toDisplayString(),
          `${reconciliationLabels[item.kind]} № ${item.number}`,
          item.own_company_name,
          formatFixed(item.debit_minor, 2),
          formatFixed(item.credit_minor, 2),
          formatFixed(item.balance_minor, 2),
        ]),
        [
          '',
          'Обороты / конечное сальдо',
          '',
          formatFixed(total.debit_minor, 2),
          formatFixed(total.credit_minor, 2),
          formatFixed(total.closing_minor, 2),
        ],
      ],
      theme: 'grid',
      styles: { font: 'NotoSans', fontSize: 8, cellPadding: 2 },
      headStyles: { fillColor: [238, 243, 250], textColor: [35, 49, 70], fontStyle: 'bold' },
      columnStyles: {
        0: { cellWidth: 24 },
        1: { cellWidth: 75 },
        3: { halign: 'right' },
        4: { halign: 'right' },
        5: { halign: 'right' },
      },
    });
    y = (doc as jsPDF & { lastAutoTable: { finalY: number } }).lastAutoTable.finalY + 10;
  }
  doc.save('Акт_сверки.pdf');
}

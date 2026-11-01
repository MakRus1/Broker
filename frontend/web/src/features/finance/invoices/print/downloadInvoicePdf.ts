import { loadPdfFonts } from '../../../../shared/printing/pdfFonts';
import { createInvoicePdf, type InvoicePrintData } from './invoicePdf';
export async function invoicePdfDocument(data: InvoicePrintData) {
  return createInvoicePdf(data, await loadPdfFonts());
}
export function invoicePdfFilename(data: Pick<InvoicePrintData, 'invoice'>) {
  const number = data.invoice.number.replace(/[\\/:*?"<>|\u0000-\u001f]/g, '_').slice(0, 100);
  return `Счёт_${number}_${data.invoice.invoice_date}.pdf`;
}
export async function downloadInvoicePdf(data: InvoicePrintData) {
  const doc = await invoicePdfDocument(data);
  doc.save(invoicePdfFilename(data));
}

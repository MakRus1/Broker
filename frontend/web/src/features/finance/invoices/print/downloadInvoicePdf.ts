import { interfaceLocale, interfaceText } from '../../../../shared/localization/interfaceText';
import { loadPdfFonts } from '../../../../shared/printing/pdfFonts';
import { createInvoicePdf, type InvoicePrintData } from './invoicePdf';
export async function invoicePdfDocument(data: InvoicePrintData) {
  return createInvoicePdf(data, await loadPdfFonts(), interfaceLocale());
}
export function invoicePdfFilename(data: Pick<InvoicePrintData, 'invoice'>) {
  const number = data.invoice.number.replace(/[\\/:*?"<>|\u0000-\u001f]/g, '_').slice(0, 100);
  return `${interfaceText('print.invoicePrefix')}_${number}_${data.invoice.invoice_date}.pdf`;
}
export async function downloadInvoicePdf(data: InvoicePrintData) {
  const doc = await invoicePdfDocument(data);
  doc.save(invoicePdfFilename(data));
}

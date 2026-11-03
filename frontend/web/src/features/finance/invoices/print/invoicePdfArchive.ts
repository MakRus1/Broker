import { interfaceText } from '../../../../shared/localization/interfaceText';
import { strToU8, zipSync } from 'fflate';
import { request } from '../../../../shared/api/http';
import type { Session } from '../../../../shared/api/session';
import type { Invoice } from '../invoices';
import type { InvoicePrintData } from './invoicePdf';
export const maxArchiveInvoices = 100;
export const maxArchiveBytes = 50 * 1024 * 1024;
export type ArchiveFailure = { id: string; number: string; message: string };
export type ArchiveResult = { blob?: Blob; ready: number; failures: ArchiveFailure[] };
export function invoiceArchiveFilename(invoice: Pick<Invoice, 'id' | 'number' | 'invoice_date'>) {
  const number = invoice.number.replace(/[\\/:*?"<>|\u0000-\u001f]/g, '_').slice(0, 70);
  return `${interfaceText('print.invoicePrefix')}_${number}_${invoice.invoice_date}_${invoice.id}.pdf`;
}
async function render(data: InvoicePrintData) {
  const { invoicePdfDocument } = await import('./downloadInvoicePdf');
  return new Uint8Array((await invoicePdfDocument(data)).output('arraybuffer'));
}
export async function buildInvoiceArchive(
  invoices: Invoice[],
  session: Session,
  signal: AbortSignal,
  onProgress: (completed: number) => void,
  renderPdf: (data: InvoicePrintData) => Promise<Uint8Array> = render,
): Promise<ArchiveResult> {
  if (!invoices.length || invoices.length > maxArchiveInvoices)
    throw new Error(interfaceText('print.selectRange', { max: maxArchiveInvoices }));
  if (new Set(invoices.map((invoice) => invoice.id)).size !== invoices.length)
    throw new Error(interfaceText('print.duplicates'));
  const files: Record<string, Uint8Array> = {};
  const failures: ArchiveFailure[] = [];
  let bytes = 0;
  let ready = 0;
  for (const [index, invoice] of invoices.entries()) {
    signal.throwIfAborted();
    try {
      if (invoice.direction !== 'outgoing') throw new Error(interfaceText('print.outgoingOnly'));
      const data = await request<InvoicePrintData>(
        '/api/v1/invoices/print-data',
        session,
        { id: invoice.id, revision: invoice.revision },
        signal,
      );
      signal.throwIfAborted();
      if (data.invoice.id !== invoice.id || data.invoice.revision !== invoice.revision)
        throw new Error(interfaceText('print.revisionChanged'));
      const pdf = await renderPdf(data);
      signal.throwIfAborted();
      if (bytes + pdf.byteLength > maxArchiveBytes)
        throw new Error(interfaceText('print.archiveLimit'));
      bytes += pdf.byteLength;
      files[invoiceArchiveFilename(invoice)] = pdf;
      ready += 1;
    } catch (error) {
      signal.throwIfAborted();
      failures.push({
        id: invoice.id,
        number: invoice.number,
        message: error instanceof Error ? error.message : interfaceText('print.pdfFailed'),
      });
    }
    onProgress(index + 1);
    // Между PDF даём браузеру отрисовать прогресс и обработать отмену.
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
  }
  signal.throwIfAborted();
  if (!ready) return { ready, failures };
  if (failures.length)
    files[interfaceText('print.errorFile')] = strToU8(
      failures
        .map((item) =>
          interfaceText('print.errorLine', {
            number: item.number,
            id: item.id,
            message: item.message,
          }),
        )
        .join('\n'),
    );
  // PDF уже сжат генератором; повторное сжатие расходует CPU почти без выигрыша.
  const archive = zipSync(files, { level: 0 });
  return { blob: new Blob([archive], { type: 'application/zip' }), ready, failures };
}

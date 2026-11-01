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
  return `Счёт_${number}_${invoice.invoice_date}_${invoice.id}.pdf`;
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
    throw new Error(`Выберите от 1 до ${maxArchiveInvoices} счетов`);
  if (new Set(invoices.map((invoice) => invoice.id)).size !== invoices.length)
    throw new Error('Счета не должны повторяться');
  const files: Record<string, Uint8Array> = {};
  const failures: ArchiveFailure[] = [];
  let bytes = 0;
  let ready = 0;
  for (const [index, invoice] of invoices.entries()) {
    signal.throwIfAborted();
    try {
      if (invoice.direction !== 'outgoing')
        throw new Error('PDF доступен только для исходящих счетов');
      const data = await request<InvoicePrintData>(
        '/api/v1/invoices/print-data',
        session,
        { id: invoice.id, revision: invoice.revision },
        signal,
      );
      signal.throwIfAborted();
      if (data.invoice.id !== invoice.id || data.invoice.revision !== invoice.revision)
        throw new Error('Редакция счёта изменилась. Обновите реестр');
      const pdf = await renderPdf(data);
      signal.throwIfAborted();
      if (bytes + pdf.byteLength > maxArchiveBytes)
        throw new Error('Превышен лимит 50 МБ. Скачайте счета меньшими группами');
      bytes += pdf.byteLength;
      files[invoiceArchiveFilename(invoice)] = pdf;
      ready += 1;
    } catch (error) {
      signal.throwIfAborted();
      failures.push({
        id: invoice.id,
        number: invoice.number,
        message: error instanceof Error ? error.message : 'Не удалось сформировать PDF',
      });
    }
    onProgress(index + 1);
    // Между PDF даём браузеру отрисовать прогресс и обработать отмену.
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
  }
  signal.throwIfAborted();
  if (!ready) return { ready, failures };
  if (failures.length)
    files['Ошибки.txt'] = strToU8(
      failures.map((item) => `Счёт № ${item.number} (${item.id}): ${item.message}`).join('\n'),
    );
  // PDF уже сжат генератором; повторное сжатие расходует CPU почти без выигрыша.
  const archive = zipSync(files, { level: 0 });
  return { blob: new Blob([archive], { type: 'application/zip' }), ready, failures };
}

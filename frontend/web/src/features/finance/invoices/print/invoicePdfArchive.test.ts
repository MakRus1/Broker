import { beforeEach, expect, it, vi } from 'vitest';
import { strFromU8, unzipSync } from 'fflate';
import { request } from '../../../../shared/api/http';
import type { Session } from '../../../../shared/api/session';
import type { Invoice } from '../invoices';
import { buildInvoiceArchive, invoiceArchiveFilename, maxArchiveBytes } from './invoicePdfArchive';
vi.mock('../../../../shared/api/http', () => ({ request: vi.fn() }));
const first = {
  id: '11111111-1111-4111-8111-111111111111',
  number: 'ИС/1',
  invoice_date: '2026-11-01',
  direction: 'outgoing',
  revision: 3,
} as Invoice;
const second = { ...first, id: '22222222-2222-4222-8222-222222222222' };
const session = {} as Session;
const pdf = vi.fn(async () => new Uint8Array([37, 80, 68, 70]));
beforeEach(() => {
  vi.mocked(request).mockReset();
  pdf.mockClear();
  vi.mocked(request).mockImplementation(
    async (_path, _session, body) =>
      ({
        invoice: [first, second].find((invoice) => invoice.id === (body as { id: string }).id),
      }) as never,
  );
});
it('сохраняет PDF с одинаковыми номерами отдельными файлами и проверяет редакции', async () => {
  const progress = vi.fn();
  const result = await buildInvoiceArchive(
    [first, second],
    session,
    new AbortController().signal,
    progress,
    pdf,
  );
  const files = unzipSync(new Uint8Array(await result.blob!.arrayBuffer()));
  expect(Object.keys(files)).toEqual([
    invoiceArchiveFilename(first),
    invoiceArchiveFilename(second),
  ]);
  expect([...files[invoiceArchiveFilename(first)]]).toEqual([37, 80, 68, 70]);
  expect(result.ready).toBe(2);
  expect(result.failures).toEqual([]);
  expect(progress.mock.calls).toEqual([[1], [2]]);
  expect(request).toHaveBeenNthCalledWith(
    1,
    '/api/v1/invoices/print-data',
    session,
    { id: first.id, revision: 3 },
    expect.any(AbortSignal),
  );
});
it('частичная ошибка не скрывается: ZIP содержит готовые PDF и русский отчёт', async () => {
  vi.mocked(request).mockRejectedValueOnce(new Error('Нет доступа'));
  const result = await buildInvoiceArchive(
    [first, second],
    session,
    new AbortController().signal,
    () => {},
    pdf,
  );
  const files = unzipSync(new Uint8Array(await result.blob!.arrayBuffer()));
  expect(result.ready).toBe(1);
  expect(Object.keys(files)).toEqual([invoiceArchiveFilename(second), 'Ошибки.txt']);
  expect(strFromU8(files['Ошибки.txt'])).toContain(`Счёт № ИС/1 (${first.id}): Нет доступа`);
});
it('не создаёт пустой архив при отказе всех счетов', async () => {
  vi.mocked(request).mockRejectedValue(new Error('Нет доступа'));
  const result = await buildInvoiceArchive(
    [first],
    session,
    new AbortController().signal,
    () => {},
    pdf,
  );
  expect(result.blob).toBeUndefined();
  expect(result.failures).toHaveLength(1);
});
it('отмена останавливает оставшиеся запросы без частичного скачивания', async () => {
  const controller = new AbortController();
  await expect(
    buildInvoiceArchive([first, second], session, controller.signal, () => controller.abort(), pdf),
  ).rejects.toThrow();
  expect(request).toHaveBeenCalledTimes(1);
});
it('не печатает устаревшую редакцию или входящий счёт', async () => {
  vi.mocked(request).mockResolvedValueOnce({ invoice: { ...first, revision: 4 } });
  const result = await buildInvoiceArchive(
    [first, { ...second, direction: 'incoming' }],
    session,
    new AbortController().signal,
    () => {},
    pdf,
  );
  expect(result.ready).toBe(0);
  expect(result.failures).toHaveLength(2);
  expect(pdf).not.toHaveBeenCalled();
});
it('отсекает пустой, повторный и слишком большой набор до запросов', async () => {
  for (const invoices of [[], [first, first], Array.from({ length: 101 }, () => first)])
    await expect(
      buildInvoiceArchive(invoices, session, new AbortController().signal, () => {}, pdf),
    ).rejects.toThrow();
  expect(request).not.toHaveBeenCalled();
});
it('ограничивает общий объём PDF в архиве', async () => {
  const result = await buildInvoiceArchive(
    [first],
    session,
    new AbortController().signal,
    () => {},
    async () => new Uint8Array(maxArchiveBytes + 1),
  );
  expect(result.ready).toBe(0);
  expect(result.failures[0].message).toContain('50 МБ');
});
it('номер счёта не может создать вложенные пути внутри ZIP', () => {
  const name = invoiceArchiveFilename({ ...first, number: '../../x\\y:bad' });
  expect(name).not.toMatch(/[\\/]/);
  expect(name).toContain(first.id);
});

import { useEffect, useState } from 'react';
import { Dialog } from '../../../../shared/ui/Dialog';
import type { Session } from '../../../../shared/api/session';
import type { Invoice } from '../invoices';
import type { ArchiveResult } from './invoicePdfArchive';
import './invoicePrint.css';
export function InvoicePdfArchiveDialog({
  invoices,
  session,
  onClose,
}: {
  invoices: Invoice[];
  session: Session;
  onClose: () => void;
}) {
  const [completed, setCompleted] = useState(0);
  const [result, setResult] = useState<ArchiveResult>();
  const [failure, setFailure] = useState('');
  const [attempt, setAttempt] = useState(0);
  useEffect(() => {
    const controller = new AbortController();
    setCompleted(0);
    setResult(undefined);
    setFailure('');
    void import('./invoicePdfArchive')
      .then(({ buildInvoiceArchive }) =>
        buildInvoiceArchive(invoices, session, controller.signal, (value) => {
          if (!controller.signal.aborted) setCompleted(value);
        }),
      )
      .then((value) => {
        if (!controller.signal.aborted) setResult(value);
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      });
    return () => controller.abort();
  }, [invoices, session, attempt]);
  const busy = !result && !failure;
  return (
    <Dialog title="Скачать счета в ZIP" onClose={onClose} className="invoice-archive-dialog">
      <p role="status" aria-live="polite">
        {busy
          ? `Формируем PDF: ${completed} из ${invoices.length}`
          : `Готово PDF: ${result?.ready ?? 0} из ${invoices.length}`}
      </p>
      {busy && <progress max={invoices.length} value={completed} aria-label="Формирование PDF" />}
      {result && result.failures.length > 0 && (
        <section>
          <h3>Не удалось сформировать: {result.failures.length}</h3>
          <ul className="invoice-archive-errors">
            {result.failures.map((item) => (
              <li key={item.id}>
                <strong>Счёт № {item.number}</strong>
                <span className="service-secondary">{item.message}</span>
              </li>
            ))}
          </ul>
          {result.ready > 0 && (
            <p className="hint">Архив содержит только готовые PDF и файл «Ошибки.txt».</p>
          )}
        </section>
      )}
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <div className="confirmation-actions">
        <button onClick={onClose}>{busy ? 'Отменить' : 'Закрыть'}</button>
        {!busy && (failure || !!result?.failures.length) && (
          <button onClick={() => setAttempt((old) => old + 1)}>Повторить формирование</button>
        )}
        {result?.blob && (
          <button
            className="primary"
            onClick={() => {
              const url = URL.createObjectURL(result.blob!);
              const link = document.createElement('a');
              link.href = url;
              link.download = 'Счета.zip';
              document.body.append(link);
              link.click();
              link.remove();
              setTimeout(() => URL.revokeObjectURL(url), 30000);
            }}
          >
            {result.failures.length ? `Скачать готовые PDF (${result.ready})` : 'Скачать ZIP'}
          </button>
        )}
      </div>
    </Dialog>
  );
}

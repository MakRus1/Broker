import { useLocalization } from '../../../localization/Localization';
import { useEffect, useState } from 'react';
import { Dialog, DialogActions } from '../../../../shared/ui/Dialog';
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
  const { translate } = useLocalization();
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
    <Dialog
      title={translate('print.invoicesArchive')}
      onClose={onClose}
      className="invoice-archive-dialog"
    >
      <p role="status" aria-live="polite">
        {busy
          ? translate('print.preparingCount', { done: completed, total: invoices.length })
          : translate('print.readyCount', { done: result?.ready ?? 0, total: invoices.length })}
      </p>
      {busy && (
        <progress
          max={invoices.length}
          value={completed}
          aria-label={translate('print.preparing')}
        />
      )}
      {result && result.failures.length > 0 && (
        <section>
          <h3>{translate('print.failedCount', { count: result.failures.length })}</h3>
          <ul className="invoice-archive-errors">
            {result.failures.map((item) => (
              <li key={item.id}>
                <strong>{translate('print.invoiceNumber', { number: item.number })}</strong>
                <span className="service-secondary">{item.message}</span>
              </li>
            ))}
          </ul>
          {result.ready > 0 && <p className="hint">{translate('print.partialArchive')}</p>}
        </section>
      )}
      <DialogActions error={failure}>
        <button onClick={onClose}>
          {busy ? translate('action.cancel') : translate('ui.close')}
        </button>
        {!busy && (failure || !!result?.failures.length) && (
          <button onClick={() => setAttempt((old) => old + 1)}>{translate('print.retry')}</button>
        )}
        {result?.blob && (
          <button
            className="primary"
            onClick={() => {
              const url = URL.createObjectURL(result.blob!);
              const link = document.createElement('a');
              link.href = url;
              link.download = translate('print.archiveName');
              document.body.append(link);
              link.click();
              link.remove();
              setTimeout(() => URL.revokeObjectURL(url), 30000);
            }}
          >
            {result.failures.length
              ? translate('print.downloadReady', { count: result.ready })
              : translate('print.downloadZip')}
          </button>
        )}
      </DialogActions>
    </Dialog>
  );
}

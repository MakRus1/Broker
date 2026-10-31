import './invoicePrint.css';
import { useState } from 'react';
import { request } from '../../../../shared/api/http';
import type { Session } from '../../../../shared/api/session';
import type { Invoice } from '../invoices';
import type { InvoicePrintData } from './invoicePdf';
export function InvoicePdfButton({ invoice, session }: { invoice: Invoice; session: Session }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  return (
    <div>
      <button
        type="button"
        disabled={busy}
        onClick={async () => {
          setBusy(true);
          setError('');
          try {
            const data = await request<InvoicePrintData>('/api/v1/invoices/print-data', session, {
              id: invoice.id,
              revision: invoice.revision,
            });
            const { downloadInvoicePdf } = await import('./downloadInvoicePdf');
            await downloadInvoicePdf(data);
          } catch (error) {
            setError(error instanceof Error ? error.message : 'Не удалось скачать PDF');
          } finally {
            setBusy(false);
          }
        }}
      >
        {busy ? 'Формируется PDF…' : 'Скачать PDF'}
      </button>
      {error && (
        <p className="error" role="alert">
          {error}
        </p>
      )}
    </div>
  );
}

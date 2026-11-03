import { useLocalization } from '../../localization/Localization';
import { InvoicePaymentPicker } from './InvoicePaymentPicker';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { formatFixed } from '../services/serviceCharges';
import type { InvoiceBalance } from '../payments/payments';
export function InvoiceSettlement({
  id,
  revision,
  active,
  session,
  onReadOnly,
}: {
  id: string;
  revision: number;
  active: boolean;
  session: Session;
  onReadOnly: (value: boolean | undefined) => void;
}) {
  const { translate } = useLocalization();
  const [value, setValue] = useState<InvoiceBalance>();
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const [picker, setPicker] = useState(false);
  useEffect(() => {
    if (!active) return;
    const controller = new AbortController();
    setFailure('');
    onReadOnly(undefined);
    request<InvoiceBalance>('/api/v1/payments/invoice-balance', session, { id }, controller.signal)
      .then((v) => {
        if (!controller.signal.aborted) {
          setValue(v);
          onReadOnly(v.paid_minor > 0);
        }
      })
      .catch((e) => {
        if (!controller.signal.aborted) setFailure(e.message);
      });
    return () => controller.abort();
  }, [id, revision, active, session, version, onReadOnly]);
  return (
    <section className="invoice-settlement">
      <div className="section-heading">
        <h2>{translate('registryField.payment_status')}</h2>
        <div className="actions">
          {value &&
            !value.invoice.archived &&
            value.outstanding_minor > 0 &&
            session.permissions.includes('finance.write_payments') && (
              <button className="primary" onClick={() => setPicker(true)}>
                {translate('forms.allocatePayment')}
              </button>
            )}
        </div>
      </div>
      {failure ? (
        <p className="error" role="alert">
          {failure}
        </p>
      ) : value ? (
        <>
          <dl className="record-fields">
            <div>
              <dt>{translate('registryField.paid_minor')}</dt>
              <dd>
                {formatFixed(value.paid_minor, 2)} {value.invoice.currency}
              </dd>
            </div>
            <div>
              <dt>{translate('registryField.outstanding_minor')}</dt>
              <dd>
                {formatFixed(value.outstanding_minor, 2)} {value.invoice.currency}
              </dd>
            </div>
          </dl>
          {value.payments.length > 0 && (
            <div className="invoice-payment-links">
              {value.payments.map((item) => (
                <div key={item.payment.id}>
                  <a
                    className="link"
                    href={`#/${item.payment.kind === 'receipt' ? 'receipts' : 'disbursements'}/${item.payment.id}`}
                  >
                    {translate('finance.invoiceNumberDate', {
                      number: item.payment.number,
                      date: CalendarDate.fromISO(item.payment.payment_date).toDisplayString(),
                    })}
                  </a>
                  <span>
                    {formatFixed(item.amount_minor, 2)} {item.payment.currency}
                  </span>
                </div>
              ))}
            </div>
          )}
        </>
      ) : (
        <p className="hint">{translate('forms.loadingPayments')}</p>
      )}
      {picker && value && (
        <InvoicePaymentPicker
          balance={value}
          session={session}
          onClose={() => setPicker(false)}
          onSaved={() => {
            setPicker(false);
            setVersion((v) => v + 1);
          }}
        />
      )}
    </section>
  );
}

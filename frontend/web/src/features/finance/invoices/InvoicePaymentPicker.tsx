import { useLocalization } from '../../localization/Localization';
import { DecimalInput } from '../../../shared/ui/DecimalInput';
import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { Dialog, DialogActions } from '../../../shared/ui/Dialog';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { formatFixed, parseFixed } from '../services/serviceCharges';
import {
  invoicePaymentLabels,
  automaticInvoiceAmounts,
  invoicePaymentAdditions,
  saveInvoicePayments,
  type PaymentTarget,
} from './invoicePayments';
import type { Payment, PaymentDetails } from '../payments/payments';
type Available = { payment: Payment; unallocated_minor: number };
export function InvoicePaymentPicker({
  balance,
  balances,
  session,
  onClose,
  onSaved,
}: {
  balance: PaymentTarget;
  balances?: PaymentTarget[];
  session: Session;
  onClose: () => void;
  onSaved: () => void;
}) {
  const { translate, locale } = useLocalization();
  const [items, setItems] = useState<Available[]>([]);
  const [after, setAfter] = useState<string | null>();
  const [query, setQuery] = useState('');
  const [selected, setSelected] = useState<Available>();
  const targets = balances ?? [balance];
  const labels = invoicePaymentLabels(locale)[balance.invoice.direction];
  const [amounts, setAmounts] = useState<Record<string, string>>({});
  const [details, setDetails] = useState<PaymentDetails>();
  const [selecting, setSelecting] = useState(false);
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  let allocationProblem: string | undefined;
  let total = 0n;
  if (details) {
    try {
      const additions = invoicePaymentAdditions(
        targets,
        amounts,
        details.unallocated_minor,
        locale,
      );
      total = additions.reduce((sum, item) => sum + BigInt(item.amount_minor), 0n);
    } catch (error) {
      allocationProblem = (error as Error).message;
      total = Object.values(amounts).reduce(
        (sum, amount) => sum + BigInt(parseFixed(amount, 2, Number.MAX_SAFE_INTEGER) ?? 0),
        0n,
      );
    }
  }

  useEffect(() => {
    const controller = new AbortController();
    setItems([]);
    setAfter(undefined);
    setLoading(true);
    const timer = setTimeout(() => {
      setLoading(true);
      setFailure('');
      request<{ items: Available[]; next_after: string | null }>(
        '/api/v1/payments/payment-candidates',
        session,
        { invoice_id: balance.invoice.id, query: query || undefined, limit: 40 },
        controller.signal,
      )
        .then((r) => {
          if (!controller.signal.aborted) {
            setItems(r.items);
            setAfter(r.next_after);
          }
        })
        .catch((e) => {
          if (!controller.signal.aborted) setFailure(e.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }, 200);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [balance.invoice.id, session, query]);
  async function more() {
    if (!after || loading) return;
    setLoading(true);
    try {
      const r = await request<{ items: Available[]; next_after: string | null }>(
        '/api/v1/payments/payment-candidates',
        session,
        { invoice_id: balance.invoice.id, query: query || undefined, after, limit: 40 },
      );
      setItems((old) => [...old, ...r.items]);
      setAfter(r.next_after);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  async function choose(item: Available) {
    setSelecting(true);
    setFailure('');
    setSelected(undefined);
    setDetails(undefined);
    try {
      const value = await request<PaymentDetails>('/api/v1/payments/get', session, {
        id: item.payment.id,
      });
      if (value.payment.archived) throw new Error(labels.archived);
      setDetails(value);
      setSelected(item);
      setAmounts(automaticInvoiceAmounts(targets, value.unallocated_minor));
    } catch (error) {
      setFailure((error as Error).message);
    } finally {
      setSelecting(false);
    }
  }
  async function save() {
    if (!details || busy) return;
    setFailure('');
    try {
      const additions = invoicePaymentAdditions(
        targets,
        amounts,
        details.unallocated_minor,
        locale,
      );
      setBusy(true);
      await saveInvoicePayments(details, additions, session, locale);
      onSaved();
    } catch (error) {
      setFailure((error as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <Dialog title={labels.action} onClose={onClose} className="invoice-payment-dialog">
      <form
        data-busy={busy}
        onSubmit={(e) => {
          e.preventDefault();
          void save();
        }}
      >
        <p className="hint">
          {translate('forms.paymentSelection', {
            count: targets.length,
            currency: balance.invoice.currency,
          })}{' '}
          · {balance.invoice.own_company_name} · {balance.invoice.counterparty_name}
        </p>
        <label>
          {translate('forms.searchByPaymentNumber')}
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder={translate('forms.partOfTheNumber')}
            disabled={busy || selecting}
          />
        </label>
        <table className="payment-allocation-table">
          <thead>
            <tr>
              <th>{translate('registryField.payment')}</th>
              <th>{translate('forms.availableBalance')}</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {items.map((item) => (
              <tr key={item.payment.id}>
                <td>
                  № {item.payment.number}
                  <span className="service-secondary">
                    {translate('ui.datePrefix')}
                    {CalendarDate.fromISO(item.payment.payment_date).toDisplayString()}
                  </span>
                </td>
                <td>
                  {formatFixed(item.unallocated_minor, 2)} {item.payment.currency}
                </td>
                <td>
                  <button
                    type="button"
                    disabled={busy || selecting}
                    aria-pressed={selected?.payment.id === item.payment.id}
                    onClick={() => {
                      void choose(item);
                    }}
                  >
                    {selected?.payment.id === item.payment.id
                      ? translate('forms.selected')
                      : translate('forms.select')}
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        {!loading && !items.length && (
          <p className="empty-state">{translate('forms.noPaymentsWithAnAvailableBalanceFound')}</p>
        )}
        <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
        {details && (
          <section className="bulk-payment-preview">
            <div className="section-heading">
              <h3>{translate('forms.allocation')}</h3>
              <button
                type="button"
                disabled={busy}
                onClick={() =>
                  setAmounts(automaticInvoiceAmounts(targets, details.unallocated_minor))
                }
              >
                {translate('forms.fillInRowOrder')}
              </button>
            </div>
            <p className="hint">
              {translate('forms.availablePaymentHint', {
                amount: formatFixed(details.unallocated_minor, 2),
                currency: details.payment.currency,
              })}
            </p>
            <div className="invoice-services-table-wrap">
              <table className="payment-allocation-table">
                <thead>
                  <tr>
                    <th>{translate('registryField.invoice_number')}</th>
                    <th>{translate('registryField.outstanding_minor')}</th>
                    <th>{translate('forms.allocate')}</th>
                  </tr>
                </thead>
                <tbody>
                  {targets.map(({ invoice, outstanding_minor }) => (
                    <tr key={invoice.id}>
                      <td>
                        № {invoice.number}
                        <span className="service-secondary">
                          {translate('ui.datePrefix')}
                          {CalendarDate.fromISO(invoice.invoice_date).toDisplayString()}
                        </span>
                      </td>
                      <td>
                        {formatFixed(outstanding_minor, 2)} {invoice.currency}
                      </td>
                      <td>
                        <DecimalInput
                          aria-label={translate('forms.invoiceAllocation', {
                            number: invoice.number,
                          })}
                          required
                          precision={2}
                          value={amounts[invoice.id] ?? ''}
                          disabled={busy}
                          onChange={(event) =>
                            setAmounts((old) => ({ ...old, [invoice.id]: event.target.value }))
                          }
                        />
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <p className="hint">{translate('payment.zeroAllocationHint')}</p>
            <p>
              {translate('forms.toAllocate')}{' '}
              <strong>
                {formatFixed(total, 2)} {details.payment.currency}
              </strong>{' '}
              · {labels.remainder}:{' '}
              <strong>
                {formatFixed(BigInt(details.unallocated_minor) - total, 2)}{' '}
                {details.payment.currency}
              </strong>
            </p>
          </section>
        )}
        <DialogActions error={failure || allocationProblem || undefined}>
          <button type="button" disabled={busy || selecting} onClick={onClose}>
            {translate('action.cancel')}
          </button>
          <button
            className="primary"
            disabled={busy || selecting || !details || !!allocationProblem}
          >
            {translate('forms.allocate')}
          </button>
        </DialogActions>
      </form>
    </Dialog>
  );
}

import { useEffect, useState } from 'react';
import { request, type Session } from './api';
import { Dialog } from './components';
import { AutoLoadMore } from './AutoLoadMore';
import { CalendarDate } from './CalendarDate';
import { formatFixed, parseFixed } from './serviceCharges';
import type { InvoiceBalance, Payment, PaymentDetails } from './payments';
type Available = { payment: Payment; unallocated_minor: number };
export function InvoicePaymentPicker({
  balance,
  session,
  onClose,
  onSaved,
}: {
  balance: InvoiceBalance;
  session: Session;
  onClose: () => void;
  onSaved: () => void;
}) {
  const [items, setItems] = useState<Available[]>([]);
  const [after, setAfter] = useState<string | null>();
  const [query, setQuery] = useState('');
  const [selected, setSelected] = useState<Available>();
  const [amount, setAmount] = useState('');
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  useEffect(() => {
    const controller = new AbortController();
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
  async function save() {
    if (!selected) return;
    const minor = parseFixed(
      amount,
      2,
      Math.min(selected.unallocated_minor, balance.outstanding_minor),
    );
    if (!minor) {
      setFailure('Укажите положительную сумму в пределах остатка платежа и счёта');
      return;
    }
    setBusy(true);
    setFailure('');
    try {
      const details = await request<PaymentDetails>('/api/v1/payments/get', session, {
        id: selected.payment.id,
      });
      const current =
        details.allocations.find((a) => a.invoice_id === balance.invoice.id)?.amount_minor ?? 0;
      const p = details.payment;
      await request('/api/v1/payments/save', session, {
        id: p.id,
        kind: p.kind,
        number: p.number,
        payment_date: p.payment_date,
        own_company_id: p.own_company_id,
        counterparty_id: p.counterparty_id,
        own_bank_account_id: p.own_bank_account_id,
        counterparty_bank_account_id: p.counterparty_bank_account_id,
        currency: p.currency,
        amount_minor: p.amount_minor,
        purpose: p.purpose,
        note: p.note,
        archived: p.archived,
        expected_revision: p.revision,
        allocations: [
          ...details.allocations.filter((a) => a.invoice_id !== balance.invoice.id),
          { invoice_id: balance.invoice.id, amount_minor: current + minor },
        ],
      });
      onSaved();
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <Dialog title="Распределить оплату" onClose={onClose} className="invoice-payment-dialog">
      <form
        data-busy={busy}
        onSubmit={(e) => {
          e.preventDefault();
          void save();
        }}
      >
        <p className="hint">
          Осталось оплатить: {formatFixed(balance.outstanding_minor, 2)} {balance.invoice.currency}
        </p>
        <label>
          Поиск по номеру платежа
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Часть номера…"
            disabled={busy}
          />
        </label>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <table className="payment-allocation-table">
          <thead>
            <tr>
              <th>Платёж</th>
              <th>Свободный остаток</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {items.map((item) => (
              <tr key={item.payment.id}>
                <td>
                  № {item.payment.number}
                  <span className="service-secondary">
                    от {CalendarDate.fromISO(item.payment.payment_date).toDisplayString()}
                  </span>
                </td>
                <td>
                  {formatFixed(item.unallocated_minor, 2)} {item.payment.currency}
                </td>
                <td>
                  <button
                    type="button"
                    disabled={busy}
                    aria-pressed={selected?.payment.id === item.payment.id}
                    onClick={() => {
                      setSelected(item);
                      setAmount(
                        formatFixed(
                          Math.min(item.unallocated_minor, balance.outstanding_minor),
                          2,
                        ).replace(/\s/g, ''),
                      );
                    }}
                  >
                    {selected?.payment.id === item.payment.id ? 'Выбрано' : 'Выбрать'}
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        {!loading && !items.length && (
          <p className="empty-state">Платежи со свободным остатком не найдены</p>
        )}
        <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
        {selected && (
          <label>
            Сумма оплаты · обязательно
            <input
              required
              inputMode="decimal"
              value={amount}
              disabled={busy}
              onChange={(e) => setAmount(e.target.value)}
            />
          </label>
        )}
        <div className="confirmation-actions">
          <button type="button" disabled={busy} onClick={onClose}>
            Отменить
          </button>
          <button className="primary" disabled={busy || !selected}>
            Распределить
          </button>
        </div>
      </form>
    </Dialog>
  );
}

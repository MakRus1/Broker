import { useEffect, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { Dialog } from './components';
import { Chevron } from './Select';
import { AutoLoadMore } from './AutoLoadMore';
import { CalendarDate } from './CalendarDate';
import { DateInput } from './DateInput';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { BankAccountSelect } from './BankAccountSelect';
import type { Contract } from './ContractsPage';
import { formatFixed, type ServiceCharge } from './serviceCharges';
import {
  type Invoice,
  type InvoiceDetails,
  type InvoiceDirection,
  type ServiceChargeInvoice,
} from './invoices';

type ServiceContext = Pick<
  ServiceCharge,
  | 'direction'
  | 'counterparty_id'
  | 'counterparty_name'
  | 'contract_id'
  | 'contract_number'
  | 'currency'
>;
type Creation = {
  id: string;
  direction: InvoiceDirection;
  number: string;
  invoice_date: string;
  due_date: string | null;
  own_company_id: string;
  counterparty_id: string;
  own_bank_account_id: string | null;
  counterparty_bank_account_id: string | null;
  contract_id: string | null;
  currency: string;
  note: string | null;
  archived: boolean;
};
export type ServiceInvoiceSelection =
  { kind: 'existing'; invoice: Invoice } | { kind: 'new'; body: Creation };
type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };
const emptyParties: Counterparty[] = [];
function today() {
  const d = new Date();
  return CalendarDate.fromISO(
    `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`,
  );
}
export async function attachServiceInvoice(
  serviceChargeId: string,
  selection: ServiceInvoiceSelection,
  session: Session,
  previousInvoiceId?: string,
): Promise<InvoiceDetails> {
  const invoiceId = selection.kind === 'existing' ? selection.invoice.id : selection.body.id;
  try {
    return selection.kind === 'existing'
      ? await request<InvoiceDetails>('/api/v1/invoices/add-service-charge', session, {
          invoice_id: invoiceId,
          service_charge_id: serviceChargeId,
          expected_revision: selection.invoice.revision,
          ...(previousInvoiceId ? { previous_invoice_id: previousInvoiceId } : {}),
        })
      : await request<InvoiceDetails>('/api/v1/invoices/save', session, {
          ...selection.body,
          service_charge_ids: [serviceChargeId],
          expected_revision: 0,
          ...(previousInvoiceId ? { previous_invoice_id: previousInvoiceId } : {}),
        });
  } catch (error) {
    // A response may be lost after commit; checking the same ID makes a retry safe.
    try {
      const saved = await request<InvoiceDetails>('/api/v1/invoices/get', session, {
        id: invoiceId,
      });
      if (!saved.invoice.archived && saved.charges.some((charge) => charge.id === serviceChargeId))
        return saved;
    } catch {
      /* Preserve the original error. */
    }
    throw error;
  }
}
function CreateInvoice({
  service,
  session,
  onChoose,
  onCancel,
}: {
  service: ServiceContext;
  session: Session;
  onChoose: (selection: ServiceInvoiceSelection) => Promise<void>;
  onCancel: () => void;
}) {
  const [number, setNumber] = useState('');
  const [date, setDate] = useState<CalendarDate | null>(today);
  const [due, setDue] = useState<CalendarDate | null>(null);
  const [own, setOwn] = useState('');
  const [ownAccount, setOwnAccount] = useState<string | null>(null);
  const [partyAccount, setPartyAccount] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [loading, setLoading] = useState(!!service.contract_id);
  const [id] = useState(() => crypto.randomUUID());
  useEffect(() => {
    if (!service.contract_id) return;
    const controller = new AbortController();
    setLoading(true);
    request<{ contract: Contract }>(
      '/api/v1/contracts/get',
      session,
      { id: service.contract_id },
      controller.signal,
    )
      .then(({ contract }) => {
        if (!controller.signal.aborted) {
          setOwn(contract.own_company_id);
          setOwnAccount(contract.own_bank_account_id);
          setPartyAccount(contract.counterparty_bank_account_id);
        }
      })
      .catch((e) => {
        if (!controller.signal.aborted) setFailure(e.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [service.contract_id, session]);
  async function choose(e: FormEvent<HTMLFormElement>) {
    e.preventDefault();
    e.stopPropagation();
    if (!date || !number.trim() || !own) return;
    setBusy(true);
    setFailure('');
    try {
      await onChoose({
        kind: 'new',
        body: {
          id,
          direction: service.direction === 'income' ? 'outgoing' : 'incoming',
          number: number.trim(),
          invoice_date: date.toString(),
          due_date: due?.toString() ?? null,
          own_company_id: own,
          counterparty_id: service.counterparty_id,
          own_bank_account_id: ownAccount,
          counterparty_bank_account_id: partyAccount,
          contract_id: service.contract_id,
          currency: service.currency,
          note: null,
          archived: false,
        },
      });
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  return (
    <form
      onSubmit={choose}
      className="service-invoice-create"
      data-busy={busy}
      data-dirty={!!number || !!own}
    >
      <div className="form-grid">
        <label>
          Номер счёта · обязательно
          <input
            required
            maxLength={100}
            value={number}
            onChange={(e) => setNumber(e.target.value)}
            disabled={busy}
          />
        </label>
        <label>
          Дата счёта · обязательно
          <DateInput required value={date} onChange={setDate} disabled={busy} />
        </label>
        <CustomerSelect
          items={emptyParties}
          value={own}
          onChange={(id) => {
            setOwn(id);
            setOwnAccount(null);
          }}
          session={session}
          required
          ownCompany
          disabled={busy || loading || !!service.contract_id}
          label="Своё юридическое лицо"
        />
        <label>
          Плановая дата оплаты
          <DateInput value={due} onChange={setDue} disabled={busy} />
        </label>
        <BankAccountSelect
          owner={own}
          label="Счёт своего юрлица"
          value={ownAccount}
          currency={service.currency}
          session={session}
          onChange={setOwnAccount}
        />
        <BankAccountSelect
          owner={service.counterparty_id}
          label="Счёт контрагента"
          value={partyAccount}
          currency={service.currency}
          session={session}
          onChange={setPartyAccount}
        />
      </div>
      <p className="hint">
        {service.counterparty_name || 'Выбранный контрагент'} · {service.currency}
        {service.contract_number ? ` · Договор ${service.contract_number}` : ''}
      </p>
      {failure && (
        <p role="alert" className="error">
          {failure}
        </p>
      )}
      <div className="confirmation-actions">
        <button type="button" disabled={busy} onClick={onCancel}>
          Назад к выбору
        </button>
        <button type="submit" className="primary" disabled={busy || loading}>
          {busy ? 'Сохраняем…' : 'Использовать этот счёт'}
        </button>
      </div>
    </form>
  );
}
export function ServiceInvoiceActions({
  service,
  session,
  selection,
  onSelect,
  compact = false,
  current,
}: {
  service: ServiceContext;
  session: Session;
  compact?: boolean;
  current?: { invoice_id: string; number: string; invoice_date: string };
  selection?: ServiceInvoiceSelection;
  onSelect: (selection: ServiceInvoiceSelection | undefined) => Promise<void> | void;
}) {
  const [open, setOpen] = useState(false);
  const [creating, setCreating] = useState(false);
  const [query, setQuery] = useState('');
  const [items, setItems] = useState<Invoice[]>([]);
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const direction = service.direction === 'income' ? 'outgoing' : 'incoming';
  const filters = [
    { field: 'archived', values: ['false'] },
    { field: 'counterparty_id', values: [service.counterparty_id] },
    { field: 'currency', values: [service.currency] },
    service.contract_id
      ? { field: 'contract_id', values: [service.contract_id] }
      : { field: 'contract_id', empty: true },
    ...(query.trim() ? [{ field: 'number', query: query.trim() }] : []),
  ];
  useEffect(() => {
    if (!open || creating) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setCursor(undefined);
    const timer = setTimeout(() => {
      request<{ items: Invoice[]; next_cursor?: Cursor }>(
        '/api/v1/invoices/list',
        session,
        { direction, filters, sort: 'invoice_date', descending: true, limit: 40 },
        controller.signal,
      )
        .then((r) => {
          if (!controller.signal.aborted) {
            setItems(r.items);
            setCursor(r.next_cursor);
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
      controller.abort();
      clearTimeout(timer);
    };
  }, [
    open,
    creating,
    query,
    service.counterparty_id,
    service.currency,
    service.contract_id,
    session,
  ]);
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    try {
      const r = await request<{ items: Invoice[]; next_cursor?: Cursor }>(
        '/api/v1/invoices/list',
        session,
        { direction, filters, sort: 'invoice_date', descending: true, limit: 40, cursor },
      );
      setItems((old) => [...old, ...r.items]);
      setCursor(r.next_cursor);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  async function choose(value: ServiceInvoiceSelection) {
    if (busy) return;
    setBusy(true);
    setFailure('');
    try {
      await onSelect(value);
      setOpen(false);
      setCreating(false);
    } catch (e) {
      setFailure((e as Error).message);
      throw e;
    } finally {
      setBusy(false);
    }
  }
  const chosen = selection?.kind === 'existing' ? selection.invoice : (selection?.body ?? current);
  return (
    <div className={compact ? 'service-invoice-field compact' : 'service-invoice-field'}>
      {!compact && <span className="service-invoice-label">Счёт</span>}
      <button
        type="button"
        className={compact ? 'service-invoice-trigger compact' : 'service-invoice-trigger'}
        disabled={busy || !service.counterparty_id || !service.currency}
        aria-label={
          chosen
            ? `Счёт № ${chosen.number} от ${CalendarDate.fromISO(chosen.invoice_date).toDisplayString()}`
            : 'Выберите или создайте счёт'
        }
        onClick={() => {
          setCreating(false);
          setOpen(true);
        }}
      >
        <span className={chosen ? '' : 'service-invoice-placeholder'}>
          {chosen
            ? `№ ${chosen.number} от ${CalendarDate.fromISO(chosen.invoice_date).toDisplayString()}`
            : compact
              ? 'Выбрать / создать'
              : 'Выберите или создайте счёт'}
        </span>
        {!compact && <Chevron />}
      </button>
      {selection?.kind === 'new' && (
        <span className="service-secondary">Будет создан при сохранении услуги</span>
      )}
      {chosen && (
        <button
          className="service-invoice-clear"
          disabled={busy}
          type="button"
          onClick={async () => {
            setBusy(true);
            setFailure('');
            try {
              await onSelect(undefined);
            } catch (e) {
              setFailure((e as Error).message);
            } finally {
              setBusy(false);
            }
          }}
        >
          {current ? 'Отвязать' : 'Убрать выбор'}
        </button>
      )}
      {!open && failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {open && (
        <Dialog
          title={creating ? 'Новый счёт' : 'Счёт услуги'}
          onClose={() => {
            if (!busy) setOpen(false);
          }}
          className="service-invoice-dialog"
        >
          {creating ? (
            <CreateInvoice
              service={service}
              session={session}
              onChoose={choose}
              onCancel={() => setCreating(false)}
            />
          ) : (
            <>
              <div className="section-heading">
                <p className="hint">
                  {service.counterparty_name || 'Выбранный контрагент'} · {service.currency}
                </p>
                <button className="primary" type="button" onClick={() => setCreating(true)}>
                  Создать счёт
                </button>
              </div>
              <label className="invoice-link-search">
                Найти существующий счёт
                <input
                  value={query}
                  onChange={(e) => setQuery(e.target.value)}
                  placeholder="Часть номера счёта"
                  disabled={busy}
                />
              </label>
              <div className="invoice-link-list">
                {items.map((item) => (
                  <button
                    type="button"
                    key={item.id}
                    disabled={busy}
                    onClick={() => {
                      void choose({ kind: 'existing', invoice: item }).catch(() => {});
                    }}
                  >
                    <span>
                      <strong>
                        № {item.number} от{' '}
                        {CalendarDate.fromISO(item.invoice_date).toDisplayString()}
                      </strong>
                      <small>{item.own_company_name}</small>
                    </span>
                    <span>
                      {formatFixed(item.total_minor, 2)} {item.currency}
                    </span>
                  </button>
                ))}
              </div>
              {!loading && !items.length && (
                <p className="hint">Подходящих счетов нет. Можно создать новый.</p>
              )}
              {failure && (
                <p className="error" role="alert">
                  {failure}
                </p>
              )}
              <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
            </>
          )}
        </Dialog>
      )}
    </div>
  );
}

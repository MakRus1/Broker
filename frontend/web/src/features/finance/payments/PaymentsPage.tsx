import { DecimalInput } from '../../../shared/ui/DecimalInput';
import { useFieldValidation } from '../../../shared/forms/fieldValidation';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { DateInput } from '../../../shared/dates/DateInput';
import { CustomerSelect, type Counterparty } from '../../counterparties/CustomerSelect';
import { BankAccountSelect } from '../../counterparties/BankAccountSelect';
import { Select } from '../../../shared/ui/Select';
import { currenciesWithCurrent } from '../currencies';
import { useDiscardConfirmation } from '../../../shared/ui/Dialog';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { PaymentRegistry } from './PaymentRegistry';
import { formatFixed, parseFixed } from '../services/serviceCharges';
import {
  paymentBase,
  type PaymentKind,
  type Payment,
  type PaymentDetails,
  type InvoiceBalance,
} from './payments';
const parties: Counterparty[] = [];
const today = () => {
  const d = new Date();
  return CalendarDate.fromISO(
    `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`,
  );
};
const displayDate = (s: string) => CalendarDate.fromISO(s).toDisplayString();
type Draft = Omit<Payment, 'payment_date' | 'amount_minor'> & {
  payment_date: CalendarDate | null;
  amount: string;
};
const draftOf = (p: Payment): Draft => {
  const { amount_minor, payment_date, ...rest } = p;
  return {
    ...rest,
    payment_date: CalendarDate.fromISO(payment_date),
    amount: formatFixed(amount_minor, 2).replace(/\s/g, ''),
  };
};
function empty(kind: PaymentKind): Draft {
  return {
    id: crypto.randomUUID(),
    kind,
    number: '',
    payment_date: today(),
    own_company_id: '',
    own_company_name: '',
    counterparty_id: '',
    counterparty_name: '',
    own_bank_account_id: null,
    counterparty_bank_account_id: null,
    currency: 'RUB',
    amount: '',
    purpose: null,
    note: null,
    archived: false,
    revision: 0,
  };
}
export function PaymentsPage({
  route,
  active,
  session,
  onTitle,
  onSaved,
  onCancelNew,
}: {
  route: string;
  active: boolean;
  session: Session;
  onTitle: (s: string) => void;
  onSaved: (p: Payment) => void;
  onCancelNew: () => void;
}) {
  const kind: PaymentKind = route.startsWith('/disbursements') ? 'disbursement' : 'receipt';
  const base = paymentBase(kind);
  const receipt = kind === 'receipt';
  const title = receipt ? 'Поступление' : 'Списание';
  const id = route.slice(base.length + 1);
  const creating = id === 'new';
  const registry = route === base;
  const [value, setValue] = useState<PaymentDetails>();
  const [draft, setDraft] = useState<Draft>();
  const [allocations, setAllocations] = useState<Record<string, string>>({});
  const [invoiceMap, setInvoiceMap] = useState<Record<string, InvoiceBalance>>({});
  const [candidates, setCandidates] = useState<InvoiceBalance[]>([]);
  const [after, setAfter] = useState<string | null>();
  const [search, setSearch] = useState('');
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [candidateFailure, setCandidateFailure] = useState('');
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const validation = useFieldValidation();
  const original = useRef('');
  const editing = useRef(false);
  editing.current = !!draft;
  const dirty = !!draft && JSON.stringify({ draft, allocations }) !== original.current;
  function edit(details: PaymentDetails) {
    const next = draftOf(details.payment);
    const selected = Object.fromEntries(
      details.allocations.map((a) => [
        a.invoice_id,
        formatFixed(a.amount_minor, 2).replace(/\s/g, ''),
      ]),
    );
    setDraft(next);
    setAllocations(selected);
    original.current = JSON.stringify({ draft: next, allocations: selected });
  }
  useEffect(() => {
    if (!active || registry || editing.current) return;
    let cancelled = false;
    setFailure('');
    setDraft(undefined);
    setValue(undefined);
    setInvoiceMap({});
    if (creating) {
      const d = empty(kind);
      setDraft(d);
      setAllocations({});
      original.current = JSON.stringify({ draft: d, allocations: {} });
      return;
    }
    setLoading(true);
    request<PaymentDetails>('/api/v1/payments/get', session, { id })
      .then(async (r) => {
        if (cancelled) return;
        if (r.payment.kind !== kind) throw new Error('Платёж не найден в этом разделе');
        setValue(r);
        onTitle(r.payment.number);
        const items = await Promise.all(
          r.allocations.map((a) =>
            request<InvoiceBalance>('/api/v1/payments/invoice-balance', session, {
              id: a.invoice_id,
            }),
          ),
        );
        if (!cancelled) setInvoiceMap(Object.fromEntries(items.map((i) => [i.invoice.id, i])));
      })
      .catch((e) => {
        if (!cancelled) setFailure(e.message);
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
    // Workspace title changes must not reload a document being edited.
  }, [id, creating, registry, active, session, kind]);
  useEffect(() => {
    if (!draft?.own_company_id || !draft.counterparty_id) return;
    const controller = new AbortController();
    const timer = setTimeout(() => {
      setCandidateFailure('');
      setLoading(true);
      request<{ items: InvoiceBalance[]; next_after?: string | null }>(
        '/api/v1/payments/invoice-candidates',
        session,
        {
          kind: draft.kind,
          payment_id: draft.id,
          own_company_id: draft.own_company_id,
          counterparty_id: draft.counterparty_id,
          currency: draft.currency,
          query: search || undefined,
          limit: 40,
        },
        controller.signal,
      )
        .then((r) => {
          if (!controller.signal.aborted) {
            setCandidates(r.items);
            setAfter(r.next_after);
            setInvoiceMap((old) => ({
              ...old,
              ...Object.fromEntries(r.items.map((i) => [i.invoice.id, i])),
            }));
          }
        })
        .catch((e) => {
          if (!controller.signal.aborted) setCandidateFailure(e.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }, 200);
    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [
    draft?.kind,
    draft?.own_company_id,
    draft?.counterparty_id,
    draft?.currency,
    search,
    session,
  ]);
  async function more() {
    if (!draft || !after || loading) return;
    setLoading(true);
    try {
      const r = await request<{ items: InvoiceBalance[]; next_after?: string | null }>(
        '/api/v1/payments/invoice-candidates',
        session,
        {
          kind: draft.kind,
          payment_id: draft.id,
          own_company_id: draft.own_company_id,
          counterparty_id: draft.counterparty_id,
          currency: draft.currency,
          query: search || undefined,
          after,
          limit: 40,
        },
      );
      setCandidates((old) => [...old, ...r.items]);
      setInvoiceMap((old) => ({
        ...old,
        ...Object.fromEntries(r.items.map((i) => [i.invoice.id, i])),
      }));
      setAfter(r.next_after);
    } catch (e) {
      setCandidateFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  async function cancel() {
    confirmDiscard(dirty, () => {
      if (creating) onCancelNew();
      else setDraft(undefined);
      setFailure('');
    });
  }
  async function changeParties(patch: Partial<Draft>) {
    if (!draft) return;
    if (Object.keys(allocations).length) {
      setFailure('Перед изменением сторон или валюты уберите распределение по счетам');
      return;
    }
    setDraft({ ...draft, ...patch });
    setAllocations({});
    setInvoiceMap({});
    setCandidates([]);
    setSearch('');
  }
  async function save(e: FormEvent) {
    e.preventDefault();
    if (!draft) return;
    const amount = parseFixed(draft.amount, 2, 9000000000000000);
    if (!amount || !draft.payment_date) {
      setFailure('Укажите сумму и дату платежа');
      return;
    }
    const items = Object.entries(allocations).map(([invoice_id, text]) => ({
      invoice_id,
      amount_minor: parseFixed(text, 2, amount),
    }));
    if (items.some((i) => !i.amount_minor)) {
      setFailure('Укажите положительную сумму для каждого выбранного счёта');
      return;
    }
    if (items.reduce((s, i) => s + (i.amount_minor ?? 0), 0) > amount) {
      setFailure('Распределение превышает сумму платежа');
      return;
    }
    setBusy(true);
    setFailure('');
    try {
      const body = {
        id: draft.id,
        kind: draft.kind,
        number: draft.number,
        payment_date: draft.payment_date,
        own_company_id: draft.own_company_id,
        counterparty_id: draft.counterparty_id,
        own_bank_account_id: draft.own_bank_account_id,
        counterparty_bank_account_id: draft.counterparty_bank_account_id,
        currency: draft.currency,
        purpose: draft.purpose,
        note: draft.note,
        archived: draft.archived,
      };
      const result = await request<PaymentDetails>('/api/v1/payments/save', session, {
        ...body,
        expected_revision: draft.revision,
        amount_minor: amount,
        allocations: items,
      });
      setValue(result);
      setDraft(undefined);
      onTitle(result.payment.number);
      onSaved(result.payment);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  if (registry) return <PaymentRegistry kind={kind} active={active} session={session} />;
  const write = session.permissions.includes('finance.write_payments');
  const amount = draft
    ? (parseFixed(draft.amount, 2, 9000000000000000) ?? 0)
    : (value?.payment.amount_minor ?? 0);
  const allocated = draft
    ? Object.values(allocations).reduce((s, v) => s + (parseFixed(v, 2, 9000000000000000) ?? 0), 0)
    : (value?.allocated_minor ?? 0);
  const selected = draft
    ? Object.keys(allocations)
    : (value?.allocations.map((a) => a.invoice_id) ?? []);
  return (
    <section className="payment-page">
      {confirmation}
      <div className="payment-heading">
        <div>
          <a className="link record-back" href={`#${base}`}>
            ← {receipt ? 'К поступлениям' : 'К списаниям'}
          </a>
          <p className="eyebrow">{title}</p>
          <h1>
            {creating
              ? `Новое ${receipt ? 'поступление' : 'списание'}`
              : (value?.payment.number ?? title)}
          </h1>
          {value && (
            <p className="record-state">
              {value.payment.archived ? 'В архиве' : 'Действующее'} · Редакция{' '}
              {value.payment.revision}
            </p>
          )}
        </div>
        {!draft && value && write && (
          <button className="primary" onClick={() => edit(value)}>
            Редактировать
          </button>
        )}
      </div>
      {failure && (
        <p role="alert" className="error">
          {failure}
        </p>
      )}
      {draft ? (
        <form
          className="payment-form"
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <fieldset disabled={busy} className="record-fields">
            <label>
              Номер платежа · обязательно
              <input
                required
                maxLength={200}
                value={draft.number}
                onChange={(e) => setDraft({ ...draft, number: e.target.value })}
              />
            </label>
            <label>
              Дата платежа · обязательно
              <DateInput
                required
                value={draft.payment_date}
                onChange={(payment_date) => setDraft({ ...draft, payment_date })}
              />
            </label>
            <CustomerSelect
              required
              ownCompany
              label="Своё юридическое лицо"
              items={parties}
              disabled={busy}
              value={draft.own_company_id}
              session={session}
              onChange={(own_company_id) =>
                void changeParties({ own_company_id, own_bank_account_id: null })
              }
            />
            <CustomerSelect
              required
              label={receipt ? 'Плательщик' : 'Получатель'}
              items={parties}
              disabled={busy}
              value={draft.counterparty_id}
              session={session}
              onChange={(counterparty_id) =>
                void changeParties({ counterparty_id, counterparty_bank_account_id: null })
              }
            />
            <label>
              Сумма · обязательно
              <DecimalInput
                required
                precision={2}
                value={draft.amount}
                onChange={(e) => setDraft({ ...draft, amount: e.target.value })}
              />
            </label>
            <Select
              label="Валюта"
              searchable
              value={draft.currency}
              options={currenciesWithCurrent(draft.currency)}
              onChange={(currency) =>
                void changeParties({
                  currency,
                  own_bank_account_id: null,
                  counterparty_bank_account_id: null,
                })
              }
            />
            <BankAccountSelect
              owner={draft.own_company_id}
              label="Счёт своего юрлица"
              value={draft.own_bank_account_id}
              currency={draft.currency}
              session={session}
              onChange={(own_bank_account_id) => setDraft({ ...draft, own_bank_account_id })}
            />
            <BankAccountSelect
              owner={draft.counterparty_id}
              label={receipt ? 'Счёт плательщика' : 'Счёт получателя'}
              value={draft.counterparty_bank_account_id}
              currency={draft.currency}
              session={session}
              onChange={(counterparty_bank_account_id) =>
                setDraft({ ...draft, counterparty_bank_account_id })
              }
            />
            <label className="record-wide">
              Назначение платежа
              <textarea
                maxLength={2000}
                value={draft.purpose ?? ''}
                onChange={(e) => setDraft({ ...draft, purpose: e.target.value || null })}
              />
            </label>
            <label className="record-wide">
              Примечание
              <textarea
                maxLength={2000}
                value={draft.note ?? ''}
                onChange={(e) => setDraft({ ...draft, note: e.target.value || null })}
              />
            </label>
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={draft.archived}
                onChange={(e) => setDraft({ ...draft, archived: e.target.checked })}
              />
              В архиве
            </label>
          </fieldset>
          <section className="payment-allocation">
            <div className="section-heading">
              <h2>Распределение по счетам</h2>
              <span>
                Не распределено:{' '}
                <strong>
                  {formatFixed(amount - allocated, 2)} {draft.currency}
                </strong>
              </span>
            </div>
            {draft.archived && (
              <p className="hint">Архивный платёж не учитывается в оплате счетов.</p>
            )}
            {selected.length > 0 && (
              <table className="payment-allocation-table">
                <thead>
                  <tr>
                    <th>Счёт</th>
                    <th>Сумма оплаты</th>
                    <th />
                  </tr>
                </thead>
                <tbody>
                  {selected.map((id) => (
                    <tr key={id}>
                      <td>
                        {invoiceMap[id]
                          ? `№ ${invoiceMap[id].invoice.number} от ${displayDate(invoiceMap[id].invoice.invoice_date)}`
                          : id}
                      </td>
                      <td>
                        <DecimalInput
                          aria-label={`Оплата счёта ${invoiceMap[id]?.invoice.number ?? id}`}
                          required
                          precision={2}
                          value={allocations[id]}
                          onChange={(e) => setAllocations({ ...allocations, [id]: e.target.value })}
                        />
                      </td>
                      <td>
                        <button
                          type="button"
                          onClick={() =>
                            setAllocations((old) =>
                              Object.fromEntries(Object.entries(old).filter(([key]) => key !== id)),
                            )
                          }
                        >
                          Убрать
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
            {draft.own_company_id && draft.counterparty_id ? (
              <>
                <label>
                  Поиск по номеру счёта
                  <input
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                    placeholder="Часть номера…"
                  />
                </label>
                {candidateFailure && (
                  <p className="error" role="alert">
                    {candidateFailure}
                  </p>
                )}
                <table className="payment-allocation-table">
                  <thead>
                    <tr>
                      <th>{receipt ? 'Исходящий счёт' : 'Входящий счёт'}</th>
                      <th>Сумма</th>
                      <th>Осталось оплатить</th>
                      <th />
                    </tr>
                  </thead>
                  <tbody>
                    {candidates
                      .filter((i) => !(i.invoice.id in allocations))
                      .map((i) => (
                        <tr key={i.invoice.id}>
                          <td>
                            № {i.invoice.number} от {displayDate(i.invoice.invoice_date)}
                          </td>
                          <td>{formatFixed(i.invoice.total_minor, 2)}</td>
                          <td>{formatFixed(i.outstanding_minor, 2)}</td>
                          <td>
                            <button
                              type="button"
                              disabled={i.outstanding_minor <= 0}
                              onClick={() =>
                                setAllocations({
                                  ...allocations,
                                  [i.invoice.id]: formatFixed(
                                    Math.min(Math.max(amount - allocated, 0), i.outstanding_minor),
                                    2,
                                  ).replace(/\s/g, ''),
                                })
                              }
                            >
                              Добавить
                            </button>
                          </td>
                        </tr>
                      ))}
                  </tbody>
                </table>
                {!loading && !candidates.length && (
                  <p className="empty-state">Подходящие счета не найдены</p>
                )}
                <AutoLoadMore
                  enabled={!!after}
                  loading={loading}
                  failed={!!candidateFailure}
                  onLoad={more}
                />
              </>
            ) : (
              <p className="hint">
                Выберите стороны платежа. Сохранить {receipt ? 'поступление' : 'списание'} можно без
                распределения.
              </p>
            )}
          </section>
          <div className="form-actions">
            <button type="button" disabled={busy} onClick={() => void cancel()}>
              Отменить
            </button>
            <button className="primary" disabled={busy}>
              Сохранить
            </button>
          </div>
        </form>
      ) : value ? (
        <>
          <dl className="payment-amounts">
            <div>
              <dt>{receipt ? 'Сумма поступления' : 'Сумма списания'}</dt>
              <dd>
                {formatFixed(amount, 2)} <span>{value.payment.currency}</span>
              </dd>
            </div>
            <div>
              <dt>Распределено</dt>
              <dd>
                {formatFixed(allocated, 2)} <span>{value.payment.currency}</span>
              </dd>
            </div>
            <div>
              <dt>Не распределено</dt>
              <dd>
                {formatFixed(value.unallocated_minor, 2)} <span>{value.payment.currency}</span>
              </dd>
            </div>
          </dl>
          <dl className="payment-details">
            <div>
              <dt>Дата платежа</dt>
              <dd>{displayDate(value.payment.payment_date)}</dd>
            </div>
            <div>
              <dt>Своё юрлицо</dt>
              <dd>{value.payment.own_company_name}</dd>
            </div>
            <div>
              <dt>{receipt ? 'Плательщик' : 'Получатель'}</dt>
              <dd>{value.payment.counterparty_name}</dd>
            </div>
            {value.payment.purpose && (
              <div className="record-wide">
                <dt>Назначение платежа</dt>
                <dd>{value.payment.purpose}</dd>
              </div>
            )}
            {value.payment.note && (
              <div className="record-wide">
                <dt>Примечание</dt>
                <dd>{value.payment.note}</dd>
              </div>
            )}
          </dl>
          <h2>Оплачиваемые счета</h2>
          <table className="payment-allocation-table">
            <thead>
              <tr>
                <th>Счёт</th>
                <th>Сумма оплаты</th>
              </tr>
            </thead>
            <tbody>
              {value.allocations.map((a) => (
                <tr key={a.invoice_id}>
                  <td>
                    <a
                      className="link"
                      href={`#/${receipt ? 'outgoing' : 'incoming'}-invoices/${a.invoice_id}`}
                    >
                      {invoiceMap[a.invoice_id]
                        ? `№ ${invoiceMap[a.invoice_id].invoice.number} от ${displayDate(invoiceMap[a.invoice_id].invoice.invoice_date)}`
                        : 'Открыть счёт'}
                    </a>
                  </td>
                  <td>
                    {formatFixed(a.amount_minor, 2)} {value.payment.currency}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          {!value.allocations.length && <p className="empty-state">{title} пока не распределено</p>}
        </>
      ) : loading ? (
        <p>Загрузка…</p>
      ) : null}
    </section>
  );
}

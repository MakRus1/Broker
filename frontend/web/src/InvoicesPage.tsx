import { InvoiceSettlement } from './InvoiceSettlement';
import { InvoiceRegistry } from './InvoiceRegistry';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import type { Contract } from './ContractsPage';
import { BankAccountSelect } from './BankAccountSelect';
import { CalendarDate } from './CalendarDate';
import { DateInput } from './DateInput';
import { Select } from './Select';
import { AutoLoadMore } from './AutoLoadMore';
import { ServiceChargeLookup } from './ServiceChargeLookup';
import { useDiscardConfirmation } from './components';
import { useFieldValidation } from './fieldValidation';
import { currenciesWithCurrent } from './currencies';
import { formatFixed, type ServiceCharge } from './serviceCharges';
import {
  invoiceRegistry,
  invoiceTitle,
  type Invoice,
  type InvoiceDetails,
  type InvoiceDirection,
} from './invoices';

type Draft = Omit<Invoice, 'invoice_date' | 'due_date'> & {
  invoice_date: CalendarDate | null;
  due_date: CalendarDate | null;
};
const dateLabel = (value: string | null) =>
  value ? CalendarDate.fromISO(value).toDisplayString() : '—';
const emptyParties: Counterparty[] = [];
const today = () => {
  const d = new Date();
  return CalendarDate.fromISO(
    `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`,
  );
};
function newInvoice(direction: InvoiceDirection): Draft {
  return {
    id: crypto.randomUUID(),
    direction,
    number: '',
    invoice_date: today(),
    due_date: null,
    own_company_id: '',
    own_company_name: '',
    counterparty_id: '',
    counterparty_name: '',
    own_bank_account_id: null,
    counterparty_bank_account_id: null,
    contract_id: null,
    contract_number: null,
    currency: 'RUB',
    note: null,
    archived: false,
    revision: 0,
    net_minor: 0,
    vat_minor: 0,
    total_minor: 0,
  };
}
function draftOf(value: Invoice): Draft {
  return {
    ...value,
    invoice_date: CalendarDate.fromISO(value.invoice_date),
    due_date: value.due_date ? CalendarDate.fromISO(value.due_date) : null,
  };
}

export function InvoicesPage({
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
  onTitle: (title: string) => void;
  onSaved: (value: Invoice) => void;
  onCancelNew: () => void;
}) {
  const direction: InvoiceDirection = route.startsWith('/incoming-invoices')
    ? 'incoming'
    : 'outgoing';
  const base = invoiceRegistry(direction);
  const title = invoiceTitle(direction);
  const id = route.split('/')[2];
  const creating = id === 'new';
  const sourceEntry = creating ? route.split('/')[3] : undefined;
  const registry = !id;
  const read = session.permissions.includes('finance.read_invoices');
  const write = session.permissions.includes('finance.write_invoices');
  const [paidReadOnly, setPaidReadOnly] = useState<boolean>();
  const [details, setDetails] = useState<InvoiceDetails>();
  const [draft, setDraft] = useState<Draft>();
  const [selected, setSelected] = useState<ServiceCharge[]>([]);
  const [candidates, setCandidates] = useState<ServiceCharge[]>([]);
  const [candidateAfter, setCandidateAfter] = useState<string | null>(null);
  const [candidateLoading, setCandidateLoading] = useState(false);
  const [candidateFailure, setCandidateFailure] = useState('');
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const baseline = useRef('');
  const paymentDelay = useRef<number | null>(null);
  const automaticDue = useRef(creating);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty =
    !!draft && JSON.stringify({ draft, selected: selected.map((x) => x.id) }) !== baseline.current;
  const validation = useFieldValidation();
  const setBaseline = (d: Draft, charges: ServiceCharge[]) => {
    baseline.current = JSON.stringify({ draft: d, selected: charges.map((x) => x.id) });
  };
  useEffect(() => {
    if (!read) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    if (creating) {
      setDraft(undefined);
      void (async () => {
        const d = newInvoice(direction);
        let services: ServiceCharge[] = [];
        if (sourceEntry) {
          const { charge } = await request<{ charge: ServiceCharge }>(
            '/api/v1/service-charges/get',
            session,
            { id: sourceEntry },
            controller.signal,
          );
          if (charge.archived || (charge.direction === 'income') !== (direction === 'outgoing'))
            throw new Error('Услуга не подходит для этого счёта');
          d.counterparty_id = charge.counterparty_id;
          d.counterparty_name = charge.counterparty_name;
          d.currency = charge.currency;
          d.contract_id = charge.contract_id;
          d.contract_number = charge.contract_number;
          services = [charge];
          if (charge.contract_id) {
            const { contract } = await request<{ contract: Contract }>(
              '/api/v1/contracts/get',
              session,
              { id: charge.contract_id },
              controller.signal,
            );
            paymentDelay.current = contract.payment_delay_days;
            if (contract.payment_delay_days !== null && d.invoice_date)
              d.due_date = d.invoice_date.addDays(contract.payment_delay_days);
            d.own_company_id = contract.own_company_id;
            d.own_bank_account_id = contract.own_bank_account_id;
            d.counterparty_bank_account_id = contract.counterparty_bank_account_id;
          }
        }
        if (controller.signal.aborted) return;
        setDraft(d);
        setSelected(services);
        setBaseline(d, services);
      })()
        .catch((e) => {
          if (!controller.signal.aborted) setFailure(e.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
      return () => controller.abort();
    }
    if (registry) {
      setLoading(false);
      return;
    }
    {
      request<InvoiceDetails>('/api/v1/invoices/get', session, { id }, controller.signal)
        .then((r) => {
          if (!controller.signal.aborted) {
            setDetails(r);
            automaticDue.current = false;
            setDraft(undefined);
            onTitle(r.invoice.number);
          }
        })
        .catch((e) => {
          if (!controller.signal.aborted) setFailure(e.message);
        })
        .finally(() => {
          if (!controller.signal.aborted) setLoading(false);
        });
    }
    return () => controller.abort();
    // onTitle updates workspace state; it is deliberately not a loading dependency.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [id, sourceEntry, direction, read, session, version]);
  useEffect(() => {
    if (!active || !dirty) return;
    const handler = (e: BeforeUnloadEvent) => {
      e.preventDefault();
    };
    window.addEventListener('beforeunload', handler);
    return () => window.removeEventListener('beforeunload', handler);
  }, [active, dirty]);
  useEffect(() => {
    setCandidates([]);
    setCandidateAfter(null);
    setCandidateFailure('');
    if (!draft?.counterparty_id) return;
    const controller = new AbortController();
    setCandidateLoading(true);
    request<{ items: ServiceCharge[]; next_after: string | null }>(
      '/api/v1/invoices/candidates',
      session,
      {
        direction,
        counterparty_id: draft.counterparty_id,
        currency: draft.currency,
        contract_id: draft.contract_id,
        invoice_id: creating ? null : draft.id,
        after: null,
        limit: 50,
      },
      controller.signal,
    )
      .then((r) => {
        if (!controller.signal.aborted) {
          setCandidates(r.items);
          setCandidateAfter(r.next_after);
        }
      })
      .catch((e) => {
        if (!controller.signal.aborted) setCandidateFailure(e.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setCandidateLoading(false);
      });
    return () => controller.abort();
  }, [
    draft?.counterparty_id,
    draft?.currency,
    draft?.contract_id,
    draft?.id,
    creating,
    direction,
    session,
  ]);
  async function moreCandidates() {
    if (!draft || !candidateAfter || candidateLoading) return;
    setCandidateLoading(true);
    setCandidateFailure('');
    try {
      const r = await request<{ items: ServiceCharge[]; next_after: string | null }>(
        '/api/v1/invoices/candidates',
        session,
        {
          direction,
          counterparty_id: draft.counterparty_id,
          currency: draft.currency,
          contract_id: draft.contract_id,
          invoice_id: creating ? null : draft.id,
          after: candidateAfter,
          limit: 50,
        },
      );
      setCandidates((old) => [...old, ...r.items]);
      setCandidateAfter(r.next_after);
    } catch (e) {
      setCandidateFailure((e as Error).message);
    } finally {
      setCandidateLoading(false);
    }
  }
  function cancel() {
    confirmDiscard(dirty, () => {
      if (creating) onCancelNew();
      else setDraft(undefined);
    });
  }
  function changeParties(changes: Partial<Draft>) {
    if (!draft) return;
    if (changes.contract_id === null) {
      paymentDelay.current = null;
      if (automaticDue.current) changes = { ...changes, due_date: null };
    }
    setDraft({ ...draft, ...changes });
    if (
      (changes.counterparty_id !== undefined &&
        changes.counterparty_id !== draft.counterparty_id) ||
      (changes.currency !== undefined && changes.currency !== draft.currency) ||
      (changes.contract_id !== undefined && changes.contract_id !== draft.contract_id)
    )
      setSelected([]);
  }
  async function save(e: FormEvent<HTMLFormElement>) {
    e.preventDefault();
    if (!draft) return;
    if (!e.currentTarget.checkValidity()) {
      e.currentTarget.reportValidity();
      return;
    }
    if (!selected.length) {
      setFailure('Выберите хотя бы одну услугу');
      return;
    }
    setBusy(true);
    setFailure('');
    try {
      const {
        revision,
        own_company_name,
        counterparty_name,
        contract_number,
        net_minor,
        vat_minor,
        total_minor,
        ...body
      } = draft;
      const r = await request<InvoiceDetails>('/api/v1/invoices/save', session, {
        ...body,
        invoice_date: draft.invoice_date?.toString(),
        due_date: draft.due_date?.toString() ?? null,
        expected_revision: revision,
        service_charge_ids: selected.map((x) => x.id),
      });
      setDetails(r);
      setDraft(undefined);
      onSaved(r.invoice);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setBusy(false);
    }
  }
  if (!read) return <p className="hint">Нет доступа к счетам</p>;
  const value = details?.invoice;
  return (
    <section className="invoices-page" data-dirty={dirty} data-busy={busy}>
      {confirmation}
      {registry ? (
        <InvoiceRegistry direction={direction} active={active} session={session} />
      ) : (
        <>
          <div className="record-heading">
            <a className="link record-back" href={'#' + base}>
              ← К {direction === 'incoming' ? 'входящим' : 'исходящим'} счетам
            </a>
            <div className="section-heading">
              <div>
                {!creating && (
                  <p className="eyebrow">
                    {direction === 'incoming' ? 'ВХОДЯЩИЙ СЧЁТ' : 'ИСХОДЯЩИЙ СЧЁТ'}
                  </p>
                )}
                <h1>{creating ? 'Новый счёт' : value?.number}</h1>
                {value && !draft && (
                  <p className="record-state">
                    {value.archived ? 'В архиве' : 'Действующий'} · Редакция {value.revision}
                    {paidReadOnly ? ' · Оплачен: только чтение' : ''}
                  </p>
                )}
              </div>
              {value && !draft && write && (
                <button
                  className="primary"
                  disabled={paidReadOnly !== false}
                  title={paidReadOnly ? 'Счёт оплачен и доступен только для чтения' : undefined}
                  onClick={() => {
                    const d = draftOf(value);
                    setDraft(d);
                    setSelected(details.charges);
                    setBaseline(d, details.charges);
                  }}
                >
                  Редактировать
                </button>
              )}
            </div>
          </div>
          {draft ? (
            <form
              data-dirty={dirty}
              data-busy={busy}
              data-validation-attempted={validation.attempted}
              className="counterparty-form"
              noValidate
              onSubmit={save}
              onInvalid={validation.onInvalid}
              onInput={validation.onInput}
            >
              <div className="form-grid">
                <label>
                  Номер счёта · обязательно
                  <input
                    required
                    maxLength={100}
                    value={draft.number}
                    onChange={(e) => setDraft({ ...draft, number: e.target.value })}
                  />
                </label>
                <label>
                  Дата счёта
                  <DateInput
                    required
                    value={draft.invoice_date}
                    onChange={(invoice_date) =>
                      setDraft({
                        ...draft,
                        invoice_date,
                        due_date:
                          automaticDue.current && paymentDelay.current !== null
                            ? (invoice_date?.addDays(paymentDelay.current) ?? null)
                            : draft.due_date,
                      })
                    }
                  />
                </label>
                <CustomerSelect
                  required
                  label="Своё юридическое лицо"
                  items={emptyParties}
                  disabled={busy}
                  ownCompany
                  value={draft.own_company_id}
                  session={session}
                  onChange={(own_company_id) =>
                    changeParties({ own_company_id, own_bank_account_id: null, contract_id: null })
                  }
                />
                <CustomerSelect
                  required
                  label="Контрагент"
                  items={emptyParties}
                  disabled={busy}
                  value={draft.counterparty_id}
                  session={session}
                  onChange={(counterparty_id) =>
                    changeParties({
                      counterparty_id,
                      counterparty_bank_account_id: null,
                      contract_id: null,
                    })
                  }
                />
                <Select
                  label="Валюта"
                  value={draft.currency}
                  searchable
                  options={currenciesWithCurrent(draft.currency)}
                  onChange={(currency) =>
                    changeParties({
                      currency,
                      own_bank_account_id: null,
                      counterparty_bank_account_id: null,
                      contract_id: null,
                    })
                  }
                />
                <ServiceChargeLookup<Contract>
                  kind="contracts"
                  label="Договор"
                  value={draft.contract_id ?? ''}
                  name={draft.contract_number ?? ''}
                  session={session}
                  counterparty={draft.counterparty_id}
                  disabled={busy || !draft.counterparty_id}
                  onChange={(contract) => {
                    paymentDelay.current = contract?.payment_delay_days ?? null;
                    changeParties({
                      ...(automaticDue.current
                        ? {
                            due_date:
                              paymentDelay.current !== null
                                ? (draft.invoice_date?.addDays(paymentDelay.current) ?? null)
                                : null,
                          }
                        : {}),
                      contract_id: contract?.id ?? null,
                      contract_number: contract?.number ?? null,
                      ...(contract
                        ? {
                            own_company_id: contract.own_company_id,
                            currency: contract.currency,
                            own_bank_account_id: contract.own_bank_account_id,
                            counterparty_bank_account_id: contract.counterparty_bank_account_id,
                          }
                        : {}),
                    });
                  }}
                />
                <BankAccountSelect
                  label="Счёт своего юрлица"
                  owner={draft.own_company_id}
                  value={draft.own_bank_account_id}
                  currency={draft.currency}
                  session={session}
                  onChange={(own_bank_account_id) => setDraft({ ...draft, own_bank_account_id })}
                />
                <BankAccountSelect
                  label="Счёт контрагента"
                  owner={draft.counterparty_id}
                  value={draft.counterparty_bank_account_id}
                  currency={draft.currency}
                  session={session}
                  onChange={(counterparty_bank_account_id) =>
                    setDraft({ ...draft, counterparty_bank_account_id })
                  }
                />
                <label>
                  Плановая дата оплаты
                  <DateInput
                    value={draft.due_date}
                    onChange={(due_date) => {
                      automaticDue.current = false;
                      setDraft({ ...draft, due_date });
                    }}
                  />
                </label>
              </div>
              <section className="invoice-lines">
                <h2>Услуги</h2>
                <p className="hint">Услуги выбранного контрагента, договора и валюты.</p>
                {selected.map((charge) => (
                  <div className="invoice-charge" key={charge.id}>
                    <div>
                      <strong>{charge.label_name}</strong>
                      <span>{charge.counterparty_name}</span>
                    </div>
                    <span>
                      {formatFixed(charge.total_minor, 2)} {charge.currency}
                    </span>
                    <button
                      type="button"
                      onClick={() => setSelected(selected.filter((x) => x.id !== charge.id))}
                    >
                      Убрать
                    </button>
                  </div>
                ))}
                <details className="invoice-service-picker" open={!selected.length}>
                  <summary>Добавить услуги</summary>
                  {candidates
                    .filter((x) => !selected.some((y) => y.id === x.id))
                    .map((charge) => (
                      <label className="invoice-candidate" key={charge.id}>
                        <input
                          type="checkbox"
                          checked={false}
                          onChange={() => setSelected([...selected, charge])}
                        />
                        <span>
                          <strong>{charge.label_name}</strong>
                          <small>
                            {charge.transportation_id ? 'Перевозка' : 'Поставка'} ·{' '}
                            {charge.note ?? charge.counterparty_name}
                          </small>
                        </span>
                        <span>
                          {formatFixed(charge.total_minor, 2)} {charge.currency}
                        </span>
                      </label>
                    ))}
                  {candidateFailure && (
                    <p className="error" role="alert">
                      {candidateFailure}
                    </p>
                  )}
                  {!candidateLoading && !candidates.length && (
                    <p className="hint">Доступных услуг нет</p>
                  )}
                  <AutoLoadMore
                    enabled={!!candidateAfter}
                    loading={candidateLoading}
                    onLoad={moreCandidates}
                  />
                </details>
                <p className="invoice-total">
                  Итого с НДС:{' '}
                  <strong>
                    {formatFixed(
                      selected.reduce((n, x) => n + BigInt(x.total_minor), 0n),
                      2,
                    )}{' '}
                    {draft.currency}
                  </strong>
                </p>
              </section>
              <label>
                Примечание
                <textarea
                  maxLength={4000}
                  value={draft.note ?? ''}
                  onChange={(e) => setDraft({ ...draft, note: e.target.value || null })}
                />
              </label>
              <label className="check">
                <input
                  type="checkbox"
                  checked={draft.archived}
                  onChange={(e) => setDraft({ ...draft, archived: e.target.checked })}
                />
                В архиве
              </label>
              {failure && (
                <p className="error" role="alert">
                  {failure}
                </p>
              )}
              <div className="form-actions">
                <button type="button" disabled={busy} onClick={cancel}>
                  Отменить
                </button>
                <button type="submit" className="primary" disabled={busy}>
                  {busy ? 'Сохраняем…' : 'Сохранить'}
                </button>
              </div>
            </form>
          ) : (
            value && (
              <>
                <InvoiceSettlement
                  onReadOnly={setPaidReadOnly}
                  id={value.id}
                  revision={value.revision}
                  active={active}
                  session={session}
                />
                <dl className="invoice-details">
                  <div>
                    <dt>Своё юрлицо</dt>
                    <dd>{value.own_company_name}</dd>
                  </div>
                  <div>
                    <dt>Контрагент</dt>
                    <dd>{value.counterparty_name}</dd>
                  </div>
                  <div>
                    <dt>Дата счёта</dt>
                    <dd>{dateLabel(value.invoice_date)}</dd>
                  </div>
                  <div>
                    <dt>Плановая оплата</dt>
                    <dd>{dateLabel(value.due_date)}</dd>
                  </div>
                  <div>
                    <dt>Договор</dt>
                    <dd>{value.contract_number ?? '—'}</dd>
                  </div>
                  <div>
                    <dt>Валюта</dt>
                    <dd>{value.currency}</dd>
                  </div>
                </dl>
                <section className="invoice-lines">
                  <h2>Услуги</h2>
                  <div className="invoice-services-table-wrap">
                    <table className="invoice-services-table">
                      <thead>
                        <tr>
                          <th>Услуга</th>
                          <th>Без НДС</th>
                          <th>НДС</th>
                          <th>Итого</th>
                        </tr>
                      </thead>
                      <tbody>
                        {details.charges.map((charge) => (
                          <tr key={charge.id}>
                            <td>
                              <strong>{charge.label_name}</strong>
                              <a
                                className="link service-secondary"
                                href={
                                  '#/' +
                                  (charge.transportation_id
                                    ? 'transportations/' + charge.transportation_id
                                    : 'shipments/' + charge.shipment_id)
                                }
                              >
                                {charge.transportation_id ? 'К перевозке' : 'К поставке'}
                              </a>
                            </td>
                            <td>{formatFixed(charge.net_minor, 2)}</td>
                            <td>{formatFixed(charge.vat_minor, 2)}</td>
                            <td>
                              <strong>
                                {formatFixed(charge.total_minor, 2)} {value.currency}
                              </strong>
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                  <div className="invoice-total">
                    Без НДС: {formatFixed(value.net_minor, 2)} · НДС:{' '}
                    {formatFixed(value.vat_minor, 2)}
                    <br />
                    Итого:{' '}
                    <strong>
                      {formatFixed(value.total_minor, 2)} {value.currency}
                    </strong>
                  </div>
                </section>
                {value.note && <p>{value.note}</p>}
              </>
            )
          )}
        </>
      )}
      {!draft && failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {loading && <p className="hint">Загружаем…</p>}
    </section>
  );
}

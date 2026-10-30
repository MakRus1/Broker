import { preferenceKey, useRegistryPreference } from './registryPreferences';
import { Chevron } from './Select';
import type { ServiceChargeInvoice } from './invoices';
import {
  ServiceInvoiceActions,
  attachServiceInvoice,
  type ServiceInvoiceSelection,
} from './ServiceInvoiceActions';
import { CalendarDate } from './CalendarDate';
import { VatRateSelect } from './VatRateSelect';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { ServiceChargeLookup } from './ServiceChargeLookup';
import { Select } from './Select';
import { AutoLoadMore } from './AutoLoadMore';
import { useDiscardConfirmation } from './components';
import { currenciesWithCurrent } from './currencies';
import {
  financialLabelUnits,
  parseVatRate,
  formatVatRate,
  vatLabel,
  type FinancialLabel,
} from './financialLabels';
import type { Contract } from './ContractsPage';
import {
  formatFixed,
  inputFixed,
  parseFixed,
  type ServiceChargeScope,
  type ServiceCharge,
  type ServiceChargeSummary,
  type CurrencyTotal,
} from './serviceCharges';

type Draft = Omit<ServiceCharge, 'quantity_milli' | 'price_minor' | 'vat_rate_basis_points'> & {
  quantity: string;
  price: string;
  rate: string;
};
const emptySummary: ServiceChargeSummary = { own: [], transportations: [], combined: [] };
function draftOf(value: ServiceCharge): Draft {
  const { quantity_milli, price_minor, vat_rate_basis_points, ...rest } = value;
  return {
    ...rest,
    quantity: inputFixed(quantity_milli, 3),
    price: inputFixed(price_minor, 2),
    rate: formatVatRate(vat_rate_basis_points ?? 0),
  };
}
function newCharge(scope: ServiceChargeScope, direction: ServiceCharge['direction']): Draft {
  return draftOf({
    ...scope,
    id: crypto.randomUUID(),
    label_id: '',
    label_name: '',
    direction,
    unit: 'transportation',
    counterparty_id: '',
    counterparty_name: '',
    contract_id: null,
    contract_number: null,
    quantity_milli: 1000,
    price_minor: 0,
    price_mode: 'net',
    currency: 'RUB',
    vat_mode: 'exempt',
    vat_rate_basis_points: null,
    net_minor: 0,
    vat_minor: 0,
    total_minor: 0,
    note: null,
    archived: false,
    revision: 0,
  });
}
function Totals({ title, items }: { title: string; items: CurrencyTotal[] }) {
  return (
    <section className="finance-totals">
      <h3>{title}</h3>
      {!items.length ? (
        <p className="hint">Услуг нет</p>
      ) : (
        <dl>
          {items.map((item) => (
            <div key={item.currency}>
              <dt>{item.currency}</dt>
              <dd>
                <span>Доход с НДС</span>
                <strong>{formatFixed(item.income_minor, 2)}</strong>
              </dd>
              <dd>
                <span>Расход с НДС</span>
                <strong>{formatFixed(item.expense_minor, 2)}</strong>
              </dd>
              <dd>
                <span>Разница с НДС</span>
                <strong>
                  {formatFixed(BigInt(item.income_minor) - BigInt(item.expense_minor), 2)}
                </strong>
              </dd>
            </div>
          ))}
        </dl>
      )}
    </section>
  );
}
export function ServiceCharges({
  scope,
  session,
  customers,
}: {
  scope: ServiceChargeScope;
  session: Session;
  customers: Counterparty[];
}) {
  const read = session.permissions.includes('finance.read_service_charges');
  const write = session.permissions.includes('finance.write_service_charges');
  const [items, setItems] = useState<ServiceCharge[]>([]);
  const [invoices, setInvoices] = useState<ServiceChargeInvoice[]>([]);
  const [summary, setSummary] = useState(emptySummary);
  const [summaryOpen, setSummaryOpen] = useRegistryPreference(
    preferenceKey(
      session.subject,
      scope.transportation_id ? 'transportation-finances' : 'shipment-finances',
      'summary-open',
    ),
    false,
    (raw) => raw === true,
  );
  const [after, setAfter] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [archived, setArchived] = useState(false);
  const [version, setVersion] = useState(0);
  const [draft, setDraft] = useState<Draft>();
  const [invoiceSelection, setInvoiceSelection] = useState<ServiceInvoiceSelection>();
  const baseline = useRef('');
  const [busy, setBusy] = useState(false);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty = !!draft && (JSON.stringify(draft) !== baseline.current || !!invoiceSelection);
  useEffect(() => {
    if (!read) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setSummary(emptySummary);
    setAfter(null);
    request<{
      items: ServiceCharge[];
      next_after: string | null;
      invoices: typeof invoices;
      summary: ServiceChargeSummary;
    }>(
      '/api/v1/service-charges/list',
      session,
      { scope, after: null, limit: 50, include_archived: archived },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
          setInvoices(result.invoices);
          setSummary(result.summary);
          setAfter(result.next_after);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [scope.shipment_id, scope.transportation_id, session, read, archived, version]);
  useEffect(() => {
    const leave = (event: BeforeUnloadEvent) => {
      if (dirty || busy) {
        event.preventDefault();
        event.returnValue = '';
      }
    };
    window.addEventListener('beforeunload', leave);
    return () => window.removeEventListener('beforeunload', leave);
  }, [dirty, busy]);
  useEffect(
    () => setInvoiceSelection(undefined),
    [draft?.counterparty_id, draft?.currency, draft?.contract_id],
  );
  function edit(value: Draft) {
    setInvoiceSelection(undefined);
    baseline.current = JSON.stringify(value);
    setDraft(value);
    setFailure('');
  }
  function cancel() {
    if (!busy)
      confirmDiscard(dirty, () => {
        setDraft(undefined);
        setFailure('');
      });
  }
  function set<K extends keyof Draft>(key: K, value: Draft[K]) {
    setDraft((old) => (old ? { ...old, [key]: value } : old));
  }
  async function more() {
    if (loading || !after) return;
    setLoading(true);
    setFailure('');
    try {
      const result = await request<{
        items: ServiceCharge[];
        next_after: string | null;
        summary: ServiceChargeSummary;
        invoices: typeof invoices;
      }>('/api/v1/service-charges/list', session, {
        scope,
        after,
        limit: 50,
        include_archived: archived,
      });
      setItems((old) => [
        ...old,
        ...result.items.filter((item) => !old.some((existing) => existing.id === item.id)),
      ]);
      setAfter(result.next_after);
      setInvoices((old) => [
        ...old.filter(
          (x) => !result.invoices.some((y) => y.service_charge_id === x.service_charge_id),
        ),
        ...result.invoices,
      ]);
      setSummary(result.summary);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось загрузить услуги');
    } finally {
      setLoading(false);
    }
  }
  async function save(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!draft || busy) return;
    const quantity = parseFixed(draft.quantity, 3, 10000000);
    const price = parseFixed(draft.price, 2, 100000000);
    const rate = draft.vat_mode === 'rate' ? parseVatRate(draft.rate) : null;
    if (
      !quantity ||
      price === undefined ||
      rate === undefined ||
      !draft.label_id ||
      !draft.counterparty_id
    ) {
      setFailure('Проверьте метку, контрагента, количество, цену и ставку НДС');
      return;
    }
    setBusy(true);
    setFailure('');
    try {
      const saved = await request<{ charge: ServiceCharge }>(
        '/api/v1/service-charges/save',
        session,
        {
          id: draft.id,
          ...scope,
          label_id: draft.label_id,
          direction: draft.direction,
          unit: draft.unit,
          counterparty_id: draft.counterparty_id,
          contract_id: draft.contract_id,
          quantity_milli: quantity,
          price_minor: price,
          price_mode: draft.price_mode,
          currency: draft.currency,
          vat_mode: draft.vat_mode,
          vat_rate_basis_points: rate,
          note: draft.note,
          archived: draft.archived,
          expected_revision: draft.revision,
        },
      );
      if (invoiceSelection) {
        try {
          await attachServiceInvoice(saved.charge.id, invoiceSelection, session);
        } catch (error) {
          const persisted = draftOf(saved.charge);
          setDraft(persisted);
          baseline.current = JSON.stringify(persisted);
          setVersion((v) => v + 1);
          setFailure('Услуга сохранена, но счёт не привязан: ' + (error as Error).message);
          return;
        }
      }
      setInvoiceSelection(undefined);
      setDraft(undefined);
      setVersion((old) => old + 1);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить услугу');
    } finally {
      setBusy(false);
    }
  }
  if (!read) return null;
  return (
    <section className="finance-section" aria-label="Финансы">
      {draft ? (
        <form onSubmit={save} data-dirty={dirty} data-busy={busy} className="finance-charge-form">
          <h2>
            {draft.revision
              ? 'Редактирование услуги'
              : draft.direction === 'income'
                ? 'Новый доход'
                : 'Новый расход'}
          </h2>
          <fieldset disabled={busy} className="record-fields">
            <ServiceChargeLookup<FinancialLabel>
              session={session}
              kind="labels"
              label="Финансовая метка · обязательно"
              required
              value={draft.label_id}
              name={draft.label_name}
              disabled={busy}
              onChange={(label) => {
                if (label)
                  setDraft((old) =>
                    old
                      ? {
                          ...old,
                          label_id: label.id,
                          label_name: label.name,
                          unit: label.unit,
                          currency: label.currency,
                          vat_mode: label.vat_mode,
                          rate: formatVatRate(label.vat_rate_basis_points ?? 0),
                          contract_id: null,
                          contract_number: null,
                          ...(label.direction === 'both' ? {} : { direction: label.direction }),
                        }
                      : old,
                  );
              }}
            />
            <Select
              label="Направление"
              value={draft.direction}
              options={[
                { value: 'income', label: 'Доход' },
                { value: 'expense', label: 'Расход' },
              ]}
              onChange={(value) => set('direction', value as ServiceCharge['direction'])}
            />
            <CustomerSelect
              session={session}
              items={customers}
              value={draft.counterparty_id}
              required
              label="Контрагент · обязательно"
              disabled={busy}
              onChange={(id) =>
                setDraft((old) =>
                  old
                    ? { ...old, counterparty_id: id, contract_id: null, contract_number: null }
                    : old,
                )
              }
            />
            <ServiceChargeLookup<Contract>
              session={session}
              kind="contracts"
              label="Договор"
              value={draft.contract_id ?? ''}
              name={draft.contract_number ?? ''}
              counterparty={draft.counterparty_id}
              disabled={busy || !draft.counterparty_id}
              onChange={(contract) =>
                setDraft((old) =>
                  old
                    ? {
                        ...old,
                        contract_id: contract?.id ?? null,
                        contract_number: contract?.number ?? null,
                        ...(contract ? { currency: contract.currency } : {}),
                      }
                    : old,
                )
              }
            />
            {session.permissions.includes('finance.write_invoices') && !draft.archived && (
              <ServiceInvoiceActions
                service={draft}
                session={session}
                selection={invoiceSelection}
                current={invoices.find((x) => x.service_charge_id === draft.id)}
                onSelect={async (selection) => {
                  const previous = invoices.find((x) => x.service_charge_id === draft.id);
                  if (previous) {
                    if (selection)
                      await attachServiceInvoice(draft.id, selection, session, previous.invoice_id);
                    else
                      await request('/api/v1/invoices/detach-service-charge', session, {
                        invoice_id: previous.invoice_id,
                        service_charge_id: draft.id,
                      });
                    setInvoiceSelection(undefined);
                    setVersion((v) => v + 1);
                  } else setInvoiceSelection(selection);
                }}
              />
            )}
            <label>
              Количество · обязательно
              <input
                required
                inputMode="decimal"
                value={draft.quantity}
                onChange={(e) => {
                  e.target.setCustomValidity('');
                  set('quantity', e.target.value);
                }}
                onBlur={(e) =>
                  e.target.setCustomValidity(
                    (parseFixed(e.target.value, 3, 10000000) ?? 0) > 0
                      ? ''
                      : 'Введите количество больше нуля, до 10 000 и до 3 знаков после запятой',
                  )
                }
              />
            </label>
            <Select
              label="Единица расчёта"
              value={draft.unit}
              options={Object.entries(financialLabelUnits).map(([value, label]) => ({
                value,
                label,
              }))}
              onChange={(value) => set('unit', value as ServiceCharge['unit'])}
            />
            <label>
              Цена · обязательно
              <input
                required
                inputMode="decimal"
                value={draft.price}
                onChange={(e) => {
                  e.target.setCustomValidity('');
                  set('price', e.target.value);
                }}
                onBlur={(e) =>
                  e.target.setCustomValidity(
                    parseFixed(e.target.value, 2, 100000000) !== undefined
                      ? ''
                      : 'Введите цену от 0 до 1 000 000, до 2 знаков после запятой',
                  )
                }
              />
            </label>
            <Select
              label="Валюта"
              value={draft.currency}
              options={currenciesWithCurrent(draft.currency)}
              searchable
              onChange={(value) =>
                setDraft((old) =>
                  old ? { ...old, currency: value, contract_id: null, contract_number: null } : old,
                )
              }
            />
            <Select
              label="Цена указана"
              value={draft.price_mode}
              options={[
                { value: 'net', label: 'Без НДС' },
                { value: 'gross', label: 'С НДС' },
              ]}
              onChange={(value) => set('price_mode', value as ServiceCharge['price_mode'])}
            />
            <Select
              label="НДС"
              value={draft.vat_mode}
              options={[
                { value: 'exempt', label: 'Без НДС' },
                { value: 'rate', label: 'Ставка НДС' },
              ]}
              onChange={(value) => set('vat_mode', value as ServiceCharge['vat_mode'])}
            />
            {draft.vat_mode === 'rate' && (
              <VatRateSelect
                value={draft.rate}
                disabled={busy}
                onChange={(value) => set('rate', value)}
              />
            )}
            <label className="record-wide">
              Примечание
              <textarea
                value={draft.note ?? ''}
                onChange={(e) => set('note', e.target.value || null)}
                maxLength={4000}
              />
            </label>
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={draft.archived}
                onChange={(e) => set('archived', e.target.checked)}
              />
              В архиве
            </label>
          </fieldset>
          {failure && (
            <p className="error" role="alert">
              {failure}
            </p>
          )}
          <div className="form-actions">
            <button type="button" disabled={busy} onClick={cancel}>
              Отменить
            </button>
            <button className="primary" disabled={busy}>
              Сохранить
            </button>
          </div>
        </form>
      ) : (
        <>
          <div className="section-heading">
            <h2>Финансы</h2>
            <div className="finance-actions">
              {write && (
                <>
                  <button className="primary" onClick={() => edit(newCharge(scope, 'income'))}>
                    Добавить доходную услугу
                  </button>
                  <button onClick={() => edit(newCharge(scope, 'expense'))}>
                    Добавить расходную услугу
                  </button>
                </>
              )}
              <button disabled={loading} onClick={() => setVersion((old) => old + 1)}>
                Обновить
              </button>
            </div>
          </div>
          {summary.combined.length > 0 && (
            <section className="finance-summary-disclosure">
              <button
                type="button"
                className="finance-summary-toggle"
                aria-expanded={summaryOpen}
                onClick={() => setSummaryOpen((old) => !old)}
              >
                <span className="finance-summary-caption">
                  <Chevron />
                  <strong>Сводка</strong>
                </span>
                <span className="finance-summary-compact">
                  {summary.combined.map((item) => (
                    <span key={item.currency}>
                      <strong>{item.currency}</strong>
                      <span>Доход {formatFixed(item.income_minor, 2)}</span>
                      <span>Расход {formatFixed(item.expense_minor, 2)}</span>
                      <span>
                        Разница{' '}
                        <strong>
                          {formatFixed(BigInt(item.income_minor) - BigInt(item.expense_minor), 2)}
                        </strong>
                      </span>
                    </span>
                  ))}
                </span>
              </button>
              {summaryOpen && (
                <div className="finance-summary">
                  <Totals
                    title={scope.transportation_id ? 'Перевозка' : 'Поставка'}
                    items={summary.own}
                  />
                  {!scope.transportation_id && (
                    <>
                      <Totals title="Перевозки" items={summary.transportations} />
                      <Totals title="Всего" items={summary.combined} />
                    </>
                  )}
                </div>
              )}
            </section>
          )}
          {!scope.transportation_id && (
            <p className="hint">
              Ниже — собственные строки поставки. Услуги перевозок редактируются в перевозках и
              учитываются в сводке один раз.
            </p>
          )}
          <div className="finance-list-toolbar">
            <h3>{scope.transportation_id ? 'Услуги' : 'Услуги поставки'}</h3>
            <label className="bank-archive-toggle">
              <input
                type="checkbox"
                checked={archived}
                onChange={(e) => setArchived(e.target.checked)}
              />
              Показать архивные
            </label>
          </div>
          {failure && (
            <p role="alert" className="error">
              {failure}
            </p>
          )}
          <div className="finance-services-table-wrap">
            <table className="finance-services-table">
              <thead>
                <tr>
                  <th>Услуга / контрагент</th>
                  <th>Направление</th>
                  <th>Количество × цена</th>
                  <th>Без НДС</th>
                  <th>НДС</th>
                  <th>Итого</th>
                  <th>Счёт</th>
                </tr>
              </thead>
              <tbody>
                {items.map((item) => {
                  const invoice = invoices.find((x) => x.service_charge_id === item.id);
                  return (
                    <tr
                      key={item.id}
                      className={write ? 'service-row-openable' : undefined}
                      tabIndex={write ? 0 : undefined}
                      aria-label={write ? `Редактировать услугу «${item.label_name}»` : undefined}
                      onClick={(event) => {
                        if (
                          !write ||
                          (event.target as HTMLElement).closest(
                            'a,button,input,select,textarea,summary,details,dialog,[role="button"],[role="combobox"]',
                          ) ||
                          window.getSelection()?.toString()
                        )
                          return;
                        edit(draftOf(item));
                      }}
                      onKeyDown={(event) => {
                        if (
                          write &&
                          event.target === event.currentTarget &&
                          event.key === 'Enter'
                        ) {
                          event.preventDefault();
                          edit(draftOf(item));
                        }
                      }}
                    >
                      <td>
                        <strong>{item.label_name}</strong>
                        <span className="service-secondary">
                          {item.counterparty_name}
                          {item.contract_number ? ` · Договор ${item.contract_number}` : ''}
                          {item.archived ? ' · В архиве' : ''}
                        </span>
                        {item.note && (
                          <details className="service-note">
                            <summary>Примечание</summary>
                            <p>{item.note}</p>
                          </details>
                        )}
                      </td>
                      <td>{item.direction === 'income' ? 'Доход' : 'Расход'}</td>
                      <td className="service-numeric">
                        {formatFixed(item.quantity_milli, 3)} {financialLabelUnits[item.unit]}
                        <span className="service-secondary">
                          × {formatFixed(item.price_minor, 2)} {item.currency} ·{' '}
                          {item.price_mode === 'gross' ? 'с НДС' : 'без НДС'}
                        </span>
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.net_minor, 2)}
                        <span className="service-secondary">{item.currency}</span>
                      </td>
                      <td className="service-numeric">
                        {formatFixed(item.vat_minor, 2)}
                        <span className="service-secondary">{vatLabel(item)}</span>
                      </td>
                      <td className="service-numeric">
                        <strong>{formatFixed(item.total_minor, 2)}</strong>
                        <span className="service-secondary">{item.currency}</span>
                      </td>
                      <td>
                        {invoice ? (
                          <a
                            className="link"
                            href={`#/${invoice.direction}-invoices/${invoice.invoice_id}`}
                          >
                            № {invoice.number}
                            <span className="service-secondary">
                              от {CalendarDate.fromISO(invoice.invoice_date).toDisplayString()}
                            </span>
                          </a>
                        ) : !item.archived &&
                          session.permissions.includes('finance.write_invoices') ? (
                          <ServiceInvoiceActions
                            compact
                            service={item}
                            session={session}
                            onSelect={async (selection) => {
                              if (selection) {
                                await attachServiceInvoice(item.id, selection, session);
                                setVersion((v) => v + 1);
                              }
                            }}
                          />
                        ) : (
                          <span className="hint">Не выбран</span>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
          {!loading && !items.length && !failure && (
            <p className="empty-state">Услуги пока не добавлены</p>
          )}
          <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
        </>
      )}
      {confirmation}
    </section>
  );
}

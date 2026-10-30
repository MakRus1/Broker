import { VatRateSelect } from './VatRateSelect';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import { CustomerSelect, type Counterparty } from './CustomerSelect';
import { FinancialEntryLookup } from './FinancialEntryLookup';
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
  type EntryScope,
  type FinancialEntry,
  type EntrySummary,
  type CurrencyTotal,
} from './financialEntries';

type Draft = Omit<FinancialEntry, 'quantity_milli' | 'price_minor' | 'vat_rate_basis_points'> & {
  quantity: string;
  price: string;
  rate: string;
};
const emptySummary: EntrySummary = { own: [], transportations: [], combined: [] };
function draftOf(value: FinancialEntry): Draft {
  const { quantity_milli, price_minor, vat_rate_basis_points, ...rest } = value;
  return {
    ...rest,
    quantity: inputFixed(quantity_milli, 3),
    price: inputFixed(price_minor, 2),
    rate: formatVatRate(vat_rate_basis_points ?? 0),
  };
}
function newEntry(scope: EntryScope, direction: FinancialEntry['direction']): Draft {
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
        <p className="hint">Начислений нет</p>
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
export function FinancialEntries({
  scope,
  session,
  customers,
}: {
  scope: EntryScope;
  session: Session;
  customers: Counterparty[];
}) {
  const read = session.permissions.includes('finance.read_entries');
  const write = session.permissions.includes('finance.write_entries');
  const [items, setItems] = useState<FinancialEntry[]>([]);
  const [summary, setSummary] = useState(emptySummary);
  const [after, setAfter] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [archived, setArchived] = useState(false);
  const [version, setVersion] = useState(0);
  const [draft, setDraft] = useState<Draft>();
  const baseline = useRef('');
  const [busy, setBusy] = useState(false);
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty = !!draft && JSON.stringify(draft) !== baseline.current;
  useEffect(() => {
    if (!read) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setSummary(emptySummary);
    setAfter(null);
    request<{ items: FinancialEntry[]; next_after: string | null; summary: EntrySummary }>(
      '/api/v1/financial-entries/list',
      session,
      { scope, after: null, limit: 50, include_archived: archived },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
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
  function edit(value: Draft) {
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
        items: FinancialEntry[];
        next_after: string | null;
        summary: EntrySummary;
      }>('/api/v1/financial-entries/list', session, {
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
      setSummary(result.summary);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось загрузить начисления');
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
      await request('/api/v1/financial-entries/save', session, {
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
      });
      setDraft(undefined);
      setVersion((old) => old + 1);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить начисление');
    } finally {
      setBusy(false);
    }
  }
  if (!read) return null;
  return (
    <section className="finance-section" aria-label="Финансы">
      {draft ? (
        <form onSubmit={save} data-dirty={dirty} data-busy={busy} className="finance-entry-form">
          <h2>
            {draft.revision
              ? 'Редактирование начисления'
              : draft.direction === 'income'
                ? 'Новый доход'
                : 'Новый расход'}
          </h2>
          <fieldset disabled={busy} className="record-fields">
            <FinancialEntryLookup<FinancialLabel>
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
              onChange={(value) => set('direction', value as FinancialEntry['direction'])}
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
            <FinancialEntryLookup<Contract>
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
              onChange={(value) => set('unit', value as FinancialEntry['unit'])}
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
              onChange={(value) => set('price_mode', value as FinancialEntry['price_mode'])}
            />
            <Select
              label="НДС"
              value={draft.vat_mode}
              options={[
                { value: 'exempt', label: 'Без НДС' },
                { value: 'rate', label: 'Ставка НДС' },
              ]}
              onChange={(value) => set('vat_mode', value as FinancialEntry['vat_mode'])}
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
                  <button className="primary" onClick={() => edit(newEntry(scope, 'income'))}>
                    Добавить доход
                  </button>
                  <button onClick={() => edit(newEntry(scope, 'expense'))}>Добавить расход</button>
                </>
              )}
              <button disabled={loading} onClick={() => setVersion((old) => old + 1)}>
                Обновить
              </button>
            </div>
          </div>
          {summary.combined.length > 0 && (
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
          {!scope.transportation_id && (
            <p className="hint">
              Ниже — собственные строки поставки. Начисления перевозок редактируются в перевозках и
              учитываются в сводке один раз.
            </p>
          )}
          <div className="finance-list-toolbar">
            <h3>{scope.transportation_id ? 'Начисления' : 'Начисления поставки'}</h3>
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
          <div className="finance-entry-list">
            {items.map((item) => (
              <article key={item.id} className="finance-entry">
                <div className="section-heading">
                  <div>
                    <h3>{item.label_name}</h3>
                    <p className="hint">
                      {item.direction === 'income' ? 'Доход' : 'Расход'}
                      {item.archived ? ' · В архиве' : ''} · {item.counterparty_name}
                      {item.contract_number ? ` · Договор ${item.contract_number}` : ''}
                    </p>
                  </div>
                  {write && <button onClick={() => edit(draftOf(item))}>Редактировать</button>}
                </div>
                <dl className="finance-entry-values">
                  <div>
                    <dt>Количество × цена {item.price_mode === 'gross' ? 'с НДС' : 'без НДС'}</dt>
                    <dd>
                      {formatFixed(item.quantity_milli, 3)} {financialLabelUnits[item.unit]} ×{' '}
                      {formatFixed(item.price_minor, 2)} {item.currency}
                    </dd>
                  </div>
                  <div>
                    <dt>Без НДС</dt>
                    <dd>
                      {formatFixed(item.net_minor, 2)} {item.currency}
                    </dd>
                  </div>
                  <div>
                    <dt>НДС · {vatLabel(item)}</dt>
                    <dd>
                      {formatFixed(item.vat_minor, 2)} {item.currency}
                    </dd>
                  </div>
                  <div>
                    <dt>Итого</dt>
                    <dd>
                      <strong>
                        {formatFixed(item.total_minor, 2)} {item.currency}
                      </strong>
                    </dd>
                  </div>
                </dl>
                {item.note && <p>{item.note}</p>}
              </article>
            ))}
          </div>
          {!loading && !items.length && !failure && (
            <p className="empty-state">Начисления пока не добавлены</p>
          )}
          <AutoLoadMore enabled={!!after} loading={loading} failed={!!failure} onLoad={more} />
        </>
      )}
      {confirmation}
    </section>
  );
}

import { VatRateSelect } from './VatRateSelect';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { request, type Session } from './api';
import {
  financialLabelDirections,
  financialLabelUnits,
  formatVatRate,
  parseVatRate,
  vatLabel,
  type FinancialLabel,
} from './financialLabels';
import { Select } from './Select';
import { currenciesWithCurrent } from './currencies';
import { useFieldValidation } from './fieldValidation';
import { useDiscardConfirmation } from './components';
import { AutoLoadMore } from './AutoLoadMore';
import { useColumnFilters } from './RegistryColumnFilters';
import { filterRequest } from './registryFilters';
import { preferenceKey, useRegistryPreference } from './registryPreferences';
import { initialFinancialLabelQuery, restoreFinancialLabelQuery } from './registryQueryPreferences';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from './RegistryTable';

type Cursor = { id: string; scope: string; value: string };
type Draft = Omit<FinancialLabel, 'vat_rate_basis_points'> & { vatRate: string };
function draftOf(value: FinancialLabel): Draft {
  const { vat_rate_basis_points, ...rest } = value;
  return {
    ...rest,
    vatRate: vat_rate_basis_points === null ? '' : formatVatRate(vat_rate_basis_points),
  };
}
function newLabel(): Draft {
  return {
    id: crypto.randomUUID(),
    name: '',
    direction: 'both',
    unit: 'transportation',
    currency: 'RUB',
    vat_mode: 'exempt',
    vatRate: '',
    note: null,
    archived: false,
    revision: 0,
  };
}
export function FinancialLabelsPage({
  route,
  active,
  session,
  onError,
  onTitle,
  onSaved,
  onCancelNew,
}: {
  route: string;
  active: boolean;
  session: Session;
  onError: (error: unknown) => void;
  onTitle: (title: string) => void;
  onSaved: (value: FinancialLabel) => void;
  onCancelNew: () => void;
}) {
  const id = /^\/financial-labels\/([0-9a-f-]{36})/i.exec(route)?.[1];
  const creating = route === '/financial-labels/new';
  const editing = creating || route.endsWith('/edit');
  const read = session.permissions.includes('finance.read_financial_labels');
  const write = session.permissions.includes('finance.write_financial_labels');
  const [value, setValue] = useState<FinancialLabel>();
  const [draft, setDraft] = useState<Draft>();
  const initial = useRef('');
  const title = useRef(onTitle);
  title.current = onTitle;
  const [items, setItems] = useState<FinancialLabel[]>([]);
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(false);
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, 'financial_labels', 'query'),
    initialFinancialLabelQuery,
    restoreFinancialLabelQuery,
  );
  const filters = useColumnFilters({
    registry: 'financial-labels',
    session: read ? session : null,
    query,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  const validation = useFieldValidation();
  const { confirmDiscard, confirmation } = useDiscardConfirmation();
  const dirty =
    editing &&
    !!draft &&
    JSON.stringify(draft) !==
      (creating ? initial.current : value ? JSON.stringify(draftOf(value)) : '');
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
  useEffect(() => {
    title.current(
      creating
        ? 'Новая финансовая метка'
        : (value?.name ?? (id ? 'Финансовая метка' : 'Финансовые метки')),
    );
  }, [creating, id, value?.name]);
  useEffect(() => {
    if (creating) {
      setDraft((old) => {
        if (old) return old;
        const next = newLabel();
        initial.current = JSON.stringify(next);
        return next;
      });
      return;
    }
    if (!id || !read) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    request<{ label: FinancialLabel }>(
      '/api/v1/financial-labels/get',
      session,
      { id },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setValue(result.label);
          setDraft(draftOf(result.label));
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setFailure(error.message);
          onError(error);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [creating, id, read, session]);
  useEffect(() => {
    if (!active || !read || id || creating) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setCursor(undefined);
    request<{ items: FinancialLabel[]; next_cursor?: Cursor }>(
      '/api/v1/financial-labels/list',
      session,
      { ...query, filters: filterRequest(query.filters), limit: 50 },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted) {
          setItems(result.items);
          setCursor(result.next_cursor);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setFailure(error.message);
          onError(error);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [active, read, id, creating, query, session, version]);
  const columns: RegistryColumn<FinancialLabel>[] = [
    {
      id: 'name',
      label: 'Наименование',
      width: 320,
      required: true,
      sort: 'name',
      render: (item) => (
        <a
          className="link"
          href={`#/financial-labels/${item.id}`}
          onClick={(event) => event.stopPropagation()}
        >
          {item.name}
        </a>
      ),
    },
    {
      id: 'direction',
      label: 'Направление',
      width: 200,
      sort: 'direction',
      render: (item) => financialLabelDirections[item.direction],
    },
    {
      id: 'unit',
      label: 'Единица расчёта',
      width: 200,
      sort: 'unit',
      render: (item) => financialLabelUnits[item.unit],
    },
    {
      id: 'currency',
      label: 'Валюта',
      width: 140,
      sort: 'currency',
      render: (item) => item.currency,
    },
    { id: 'vat', label: 'НДС', width: 160, filterFields: ['vat_mode'], render: vatLabel },
    {
      id: 'archived',
      label: 'Состояние',
      width: 180,
      sort: 'archived',
      render: (item) => (item.archived ? 'В архиве' : 'Действующая'),
    },
  ];
  const columnState = useRegistryColumns(session.subject, 'financial_labels', columns);
  function cancel() {
    if (busy) return;
    confirmDiscard(dirty, () => {
      if (creating) onCancelNew();
      else {
        if (value) setDraft(draftOf(value));
        location.hash = `/financial-labels/${id}`;
      }
    });
  }
  async function save(event: FormEvent) {
    event.preventDefault();
    if (!draft || busy) return;
    const rate = draft.vat_mode === 'rate' ? parseVatRate(draft.vatRate) : null;
    if (rate === undefined) return;
    setBusy(true);
    setFailure('');
    try {
      const { revision, vatRate, ...body } = draft;
      const result = await request<{ label: FinancialLabel }>(
        '/api/v1/financial-labels/save',
        session,
        { ...body, vat_rate_basis_points: rate, expected_revision: revision },
      );
      setValue(result.label);
      setDraft(draftOf(result.label));
      onSaved(result.label);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось сохранить финансовую метку');
      onError(error);
    } finally {
      setBusy(false);
    }
  }
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const result = await request<{ items: FinancialLabel[]; next_cursor?: Cursor }>(
        '/api/v1/financial-labels/list',
        session,
        { ...query, filters: filterRequest(query.filters), limit: 50, cursor },
      );
      setItems((old) => [...old, ...result.items]);
      setCursor(result.next_cursor);
    } catch (error) {
      setFailure(error instanceof Error ? error.message : 'Не удалось загрузить метки');
      onError(error);
    } finally {
      setLoading(false);
    }
  }
  if (!read)
    return (
      <section className="record-page financial-label-page">
        <h1>Финансовые метки</h1>
        <p>Нет права на просмотр финансовых меток.</p>
      </section>
    );
  if (!id && !creating)
    return (
      <section className="record-page financial-label-page">
        <div className="section-heading">
          <h1>Финансовые метки</h1>
          {write && (
            <a className="button primary" href="#/financial-labels/new">
              Новая метка
            </a>
          )}
        </div>
        <div className="record-toolbar">
          <RegistryColumnSettings {...columnState} />
          <button disabled={loading} onClick={() => setVersion((value) => value + 1)}>
            Обновить
          </button>
        </div>
        {failure && (
          <p className="error" role="alert">
            {failure}
          </p>
        )}
        <RegistryTable
          items={items}
          columns={columnState}
          definitions={columns}
          sort={query.sort}
          descending={query.descending}
          onSort={(sort) =>
            setQuery((old) => ({
              ...old,
              sort,
              descending: old.sort === sort ? !old.descending : false,
            }))
          }
          filters={filters}
          onOpen={(item) => {
            location.hash = `/financial-labels/${item.id}`;
          }}
          emptyMessage={
            loading ? 'Загружаем метки…' : failure ? undefined : 'Финансовые метки не найдены'
          }
        />
        <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
      </section>
    );
  return (
    <section className="record-page financial-label-page">
      {confirmation}
      <div className="record-heading">
        <a className="link record-back" href="#/financial-labels">
          ← К финансовым меткам
        </a>
        <div className="section-heading">
          <div>
            {!creating && <p className="eyebrow">ФИНАНСОВАЯ МЕТКА</p>}
            <h1>{creating ? 'Новая финансовая метка' : (value?.name ?? 'Финансовая метка')}</h1>
            {value && !editing && (
              <p className="record-state">
                {value.archived ? 'В архиве' : 'Действующая'} · Редакция {value.revision}
              </p>
            )}
          </div>
          {!editing && value && write && (
            <a className="button primary" href={`#/financial-labels/${id}/edit`}>
              Редактировать
            </a>
          )}
        </div>
      </div>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      {loading && <p className="hint">Загружаем метку…</p>}
      {editing && !write && <p>Нет права на изменение финансовой метки.</p>}
      {editing && draft && write && !loading ? (
        <form
          className="counterparty-form"
          onSubmit={save}
          data-dirty={dirty}
          data-busy={busy}
          data-validation-attempted={validation.attempted}
          onInvalid={validation.onInvalid}
          onInput={validation.onInput}
        >
          <div className="record-fields">
            <label className="record-wide">
              Наименование · обязательно
              <input
                name="name"
                required
                maxLength={256}
                value={draft.name}
                disabled={busy}
                onChange={(event) => setDraft({ ...draft, name: event.target.value })}
              />
            </label>
            <Select
              label="Направление"
              value={draft.direction}
              disabled={busy}
              onChange={(direction) =>
                setDraft({ ...draft, direction: direction as FinancialLabel['direction'] })
              }
              options={Object.entries(financialLabelDirections).map(([value, label]) => ({
                value,
                label,
              }))}
            />
            <Select
              label="Единица расчёта"
              value={draft.unit}
              disabled={busy}
              onChange={(unit) => setDraft({ ...draft, unit: unit as FinancialLabel['unit'] })}
              options={Object.entries(financialLabelUnits).map(([value, label]) => ({
                value,
                label,
              }))}
            />
            <Select
              label="Валюта по умолчанию"
              value={draft.currency}
              disabled={busy}
              searchable
              onChange={(currency) => setDraft({ ...draft, currency })}
              options={currenciesWithCurrent(draft.currency)}
            />
            <VatRateSelect
              value={draft.vatRate}
              mode={draft.vat_mode}
              disabled={busy}
              onMode={(vat_mode) => setDraft((old) => (old ? { ...old, vat_mode } : old))}
              onChange={(vatRate) => setDraft((old) => (old ? { ...old, vatRate } : old))}
            />
            <label className="record-wide">
              Примечание
              <textarea
                rows={3}
                maxLength={4000}
                disabled={busy}
                value={draft.note ?? ''}
                onChange={(event) => setDraft({ ...draft, note: event.target.value || null })}
              />
            </label>
          </div>
          <label className="bank-archive-toggle">
            <input
              type="checkbox"
              checked={draft.archived}
              disabled={busy}
              onChange={(event) => setDraft({ ...draft, archived: event.target.checked })}
            />
            В архиве
          </label>
          <div className="form-actions">
            <button type="button" disabled={busy} onClick={cancel}>
              Отменить
            </button>
            <button className="primary" disabled={busy}>
              {busy ? 'Сохраняем…' : 'Сохранить'}
            </button>
          </div>
        </form>
      ) : (
        !editing &&
        value && (
          <div className="record-fields record-summary">
            <div>
              <span className="hint">Направление</span>
              <p>{financialLabelDirections[value.direction]}</p>
            </div>
            <div>
              <span className="hint">Единица расчёта</span>
              <p>{financialLabelUnits[value.unit]}</p>
            </div>
            <div>
              <span className="hint">Валюта по умолчанию</span>
              <p>{value.currency}</p>
            </div>
            <div>
              <span className="hint">НДС по умолчанию</span>
              <p>{vatLabel(value)}</p>
            </div>
            <div className="record-wide">
              <span className="hint">Примечание</span>
              <p>{value.note || '—'}</p>
            </div>
          </div>
        )
      )}
    </section>
  );
}

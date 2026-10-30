import { useEffect, useState } from 'react';
import { request, type Session } from './api';
import { CalendarDate } from './CalendarDate';
import { AutoLoadMore } from './AutoLoadMore';
import { useColumnFilters } from './RegistryColumnFilters';
import { filterRequest } from './registryFilters';
import { preferenceKey, useRegistryPreference } from './registryPreferences';
import { initialInvoiceQuery, restoreInvoiceQuery } from './registryQueryPreferences';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from './RegistryTable';
import { formatFixed } from './serviceCharges';
import { invoiceRegistry, invoiceTitle, type Invoice, type InvoiceDirection } from './invoices';
type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };
const date = (value: string | null) =>
  value ? CalendarDate.fromISO(value).toDisplayString() : '—';
export function InvoiceRegistry({
  direction,
  active,
  session,
}: {
  direction: InvoiceDirection;
  active: boolean;
  session: Session;
}) {
  const [items, setItems] = useState<Invoice[]>([]);
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const base = invoiceRegistry(direction);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, `${direction}-invoices`, 'query'),
    initialInvoiceQuery,
    restoreInvoiceQuery,
  );
  const registryRequest = { ...query, direction };
  const filters = useColumnFilters({
    registry: 'invoices',
    session,
    query: registryRequest,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  useEffect(() => {
    if (!active) return;
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setCursor(undefined);
    request<{ items: Invoice[]; next_cursor?: Cursor }>(
      '/api/v1/invoices/list',
      session,
      { ...query, direction, filters: filterRequest(query.filters), limit: 50 },
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
    return () => controller.abort();
  }, [active, direction, query, session, version]);
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const r = await request<{ items: Invoice[]; next_cursor?: Cursor }>(
        '/api/v1/invoices/list',
        session,
        { ...query, direction, filters: filterRequest(query.filters), limit: 50, cursor },
      );
      setItems((old) => [...old, ...r.items]);
      setCursor(r.next_cursor);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  const columns: RegistryColumn<Invoice>[] = [
    {
      id: 'number',
      label: 'Счёт',
      width: 220,
      required: true,
      sort: 'number',
      render: (item) => (
        <a className="link" href={`#${base}/${item.id}`} onClick={(e) => e.stopPropagation()}>
          {item.number}
        </a>
      ),
    },
    {
      id: 'counterparty',
      label: 'Контрагент',
      width: 260,
      filterFields: ['counterparty_id'],
      render: (item) => item.counterparty_name,
    },
    {
      id: 'own_company',
      label: 'Своё юрлицо',
      width: 260,
      filterFields: ['own_company_id'],
      render: (item) => item.own_company_name,
    },
    {
      id: 'invoice_date',
      label: 'Дата счёта',
      width: 180,
      sort: 'invoice_date',
      render: (item) => date(item.invoice_date),
    },
    {
      id: 'due_date',
      label: 'Плановая оплата',
      width: 180,
      sort: 'due_date',
      render: (item) => date(item.due_date),
    },
    {
      id: 'total',
      label: 'Сумма с НДС',
      width: 200,
      filterFields: ['currency'],
      render: (item) => `${formatFixed(item.total_minor, 2)} ${item.currency}`,
    },
    {
      id: 'archived',
      label: 'Состояние',
      width: 170,
      sort: 'archived',
      render: (item) => (item.archived ? 'В архиве' : 'Действующий'),
    },
  ];
  const columnState = useRegistryColumns(session.subject, `${direction}-invoices`, columns);
  return (
    <section className="invoice-registry">
      <div className="page-heading">
        <div>
          <h1>{invoiceTitle(direction)}</h1>
          <p className="hint">Загружено: {items.length}</p>
        </div>
        <div className="actions">
          <button onClick={() => setVersion((v) => v + 1)}>Обновить</button>
          {session.permissions.includes('finance.write_invoices') && (
            <a className="button primary" href={`#${base}/new`}>
              Добавить счёт
            </a>
          )}
        </div>
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
        filters={filters}
        filterSummary={
          <div className="registry-column-actions">
            <RegistryColumnSettings {...columnState} />
          </div>
        }
        sort={query.sort}
        descending={query.descending}
        onSort={(sort) =>
          setQuery((old) => ({
            ...old,
            sort,
            descending: old.sort === sort ? !old.descending : false,
          }))
        }
        onOpen={(item) => {
          location.hash = `${base}/${item.id}`;
        }}
        emptyMessage={loading ? 'Загружаем счета…' : failure ? undefined : 'Счета не найдены'}
      />
      <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
    </section>
  );
}

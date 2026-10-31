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
import { paymentBase, type Payment, type PaymentKind } from './payments';
type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };
const initialPaymentQuery = { ...initialInvoiceQuery, sort: 'payment_date' };
const restorePaymentQuery = (raw: unknown) => {
  const query = restoreInvoiceQuery(raw);
  return { ...query, sort: query.sort === 'invoice_date' ? 'payment_date' : query.sort };
};
const date = (value: string | null) =>
  value ? CalendarDate.fromISO(value).toDisplayString() : '—';
export function PaymentRegistry({
  kind,
  active,
  session,
}: {
  kind: PaymentKind;
  active: boolean;
  session: Session;
}) {
  const [items, setItems] = useState<Payment[]>([]);
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const base = paymentBase(kind);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, `${kind}-payments`, 'query'),
    initialPaymentQuery,
    restorePaymentQuery,
  );
  const registryRequest = { ...query, kind };
  const filters = useColumnFilters({
    registry: 'payments',
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
    request<{ items: Payment[]; next_cursor?: Cursor }>(
      '/api/v1/payments/list',
      session,
      { ...query, kind, filters: filterRequest(query.filters), limit: 50 },
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
  }, [active, kind, query, session, version]);
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const r = await request<{ items: Payment[]; next_cursor?: Cursor }>(
        '/api/v1/payments/list',
        session,
        { ...query, kind, filters: filterRequest(query.filters), limit: 50, cursor },
      );
      setItems((old) => [...old, ...r.items]);
      setCursor(r.next_cursor);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  const columns: RegistryColumn<Payment>[] = [
    {
      id: 'number',
      label: 'Платёж',
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
      id: 'payment_date',
      label: 'Дата платежа',
      width: 180,
      sort: 'payment_date',
      render: (item) => date(item.payment_date),
    },
    {
      id: 'total',
      label: 'Сумма',
      width: 200,
      filterFields: ['currency'],
      render: (item) => `${formatFixed(item.amount_minor, 2)} ${item.currency}`,
    },
    {
      id: 'archived',
      label: 'Состояние',
      width: 170,
      sort: 'archived',
      render: (item) => (item.archived ? 'В архиве' : 'Действующий'),
    },
  ];
  const columnState = useRegistryColumns(session.subject, `${kind}-payments`, columns);
  return (
    <section className="invoice-registry">
      <div className="page-heading">
        <div>
          <h1>{kind === 'receipt' ? 'Поступления' : 'Списания'}</h1>
          <p className="hint">Загружено: {items.length}</p>
        </div>
        <div className="actions">
          <button onClick={() => setVersion((v) => v + 1)}>Обновить</button>
          {session.permissions.includes('finance.write_payments') && (
            <a className="button primary" href={`#${base}/new`}>
              Добавить поступление
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
        emptyMessage={loading ? 'Загружаем платежи…' : failure ? undefined : 'Платежи не найдены'}
      />
      <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
    </section>
  );
}

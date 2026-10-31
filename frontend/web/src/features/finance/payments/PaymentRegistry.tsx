import { RegistryHeading } from '../../../shared/ui/RegistryHeading';
import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { useColumnFilters } from '../../registry/RegistryColumnFilters';
import { filterRequest, scopedFilters, type RegistryScope } from '../../registry/registryFilters';
import { preferenceKey, useRegistryPreference } from '../../registry/registryPreferences';
import { initialInvoiceQuery, restoreInvoiceQuery } from '../../registry/registryQueryPreferences';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from '../../registry/RegistryTable';
import { formatFixed } from '../services/serviceCharges';
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
  scope,
  refreshVersion = 0,
}: {
  kind: PaymentKind;
  active: boolean;
  session: Session;
  scope?: RegistryScope;
  refreshVersion?: number;
}) {
  const [items, setItems] = useState<Payment[]>([]);
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [version, setVersion] = useState(0);
  const base = paymentBase(kind);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, `${kind}-payments${scope ? `:${scope.key}` : ''}`, 'query'),
    initialPaymentQuery,
    restorePaymentQuery,
  );
  const registryRequest = { ...query, kind, filters: scopedFilters(query.filters, scope) };
  const filters = useColumnFilters({
    registry: 'payments',
    session,
    query: registryRequest,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  const scopedFields = new Set(scope?.filters.map((filter) => filter.field));
  const visibleFilters = {
    ...filters,
    fields: filters.fields.filter((field) => !scopedFields.has(field.id)),
    filters: filters.filters.filter((filter) => !scopedFields.has(filter.field)),
  };
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
      { ...registryRequest, filters: filterRequest(registryRequest.filters), limit: 50 },
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
  }, [active, kind, query, session, version, scope, refreshVersion]);
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const r = await request<{ items: Payment[]; next_cursor?: Cursor }>(
        '/api/v1/payments/list',
        session,
        { ...registryRequest, filters: filterRequest(registryRequest.filters), limit: 50, cursor },
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
      <RegistryHeading
        title={scope ? undefined : kind === 'receipt' ? 'Поступления' : 'Списания'}
        count={items.length}
        onRefresh={() => setVersion((v) => v + 1)}
      >
        {!scope && session.permissions.includes('finance.write_payments') && (
          <a className="button primary" href={`#${base}/new`}>
            {kind === 'receipt' ? 'Добавить поступление' : 'Добавить списание'}
          </a>
        )}
        <RegistryColumnSettings {...columnState} />
      </RegistryHeading>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <RegistryTable
        items={items}
        columns={columnState}
        definitions={columns}
        filters={visibleFilters}
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

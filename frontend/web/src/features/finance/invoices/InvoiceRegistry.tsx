import { RegistryHeading } from '../../../shared/ui/RegistryHeading';
import { useEffect, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { useColumnFilters } from '../../registry/RegistryColumnFilters';
import { filterRequest } from '../../registry/registryFilters';
import { preferenceKey, useRegistryPreference } from '../../registry/registryPreferences';
import { initialInvoiceQuery, restoreInvoiceQuery } from '../../registry/registryQueryPreferences';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from '../../registry/RegistryTable';
import { formatFixed } from '../services/serviceCharges';
import {
  invoiceRegistry,
  invoiceTitle,
  type Invoice,
  type InvoiceDirection,
  type InvoiceRegistryItem,
  invoicePaymentStatusLabel,
} from './invoices';
type InvoiceRow = Invoice & Omit<InvoiceRegistryItem, 'invoice'>;
const flatten = ({ invoice, ...settlement }: InvoiceRegistryItem): InvoiceRow => ({
  ...invoice,
  ...settlement,
});
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
  const [items, setItems] = useState<InvoiceRow[]>([]);
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
    request<{ items: InvoiceRegistryItem[]; next_cursor?: Cursor }>(
      '/api/v1/invoices/list',
      session,
      { ...query, direction, filters: filterRequest(query.filters), limit: 50 },
      controller.signal,
    )
      .then((r) => {
        if (!controller.signal.aborted) {
          setItems(r.items.map(flatten));
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
      const r = await request<{ items: InvoiceRegistryItem[]; next_cursor?: Cursor }>(
        '/api/v1/invoices/list',
        session,
        { ...query, direction, filters: filterRequest(query.filters), limit: 50, cursor },
      );
      setItems((old) => [...old, ...r.items.map(flatten)]);
      setCursor(r.next_cursor);
    } catch (e) {
      setFailure((e as Error).message);
    } finally {
      setLoading(false);
    }
  }
  const columns: RegistryColumn<InvoiceRow>[] = [
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
      sort: 'total_minor',
      render: (item) => `${formatFixed(item.total_minor, 2)} ${item.currency}`,
    },
    {
      id: 'paid_minor',
      label: 'Оплачено',
      width: 180,
      sort: 'paid_minor',
      filterFields: [],
      render: (item) => `${formatFixed(item.paid_minor, 2)} ${item.currency}`,
    },
    {
      id: 'outstanding_minor',
      label: 'Осталось оплатить',
      width: 190,
      sort: 'outstanding_minor',
      filterFields: [],
      render: (item) => `${formatFixed(item.outstanding_minor, 2)} ${item.currency}`,
    },
    {
      id: 'payment_status',
      label: 'Оплата',
      width: 210,
      sort: 'payment_status',
      render: (item) => invoicePaymentStatusLabel[item.payment_status],
    },
    {
      id: 'overdue',
      label: 'Просрочка',
      width: 160,
      sort: 'overdue_days',
      filterFields: ['overdue'],
      render: (item) =>
        item.overdue_days > 0 ? (
          <span className="invoice-overdue">{item.overdue_days} дн.</span>
        ) : (
          '—'
        ),
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
      <RegistryHeading
        title={invoiceTitle(direction)}
        count={items.length}
        onRefresh={() => setVersion((v) => v + 1)}
      >
        {session.permissions.includes('finance.write_invoices') && (
          <a className="button primary" href={`#${base}/new`}>
            Добавить счёт
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
        filters={filters}
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

import { RegistryHeading } from '../../../shared/ui/RegistryHeading';
import { useEffect, useMemo, useState } from 'react';
import { request } from '../../../shared/api/http';
import { type Session } from '../../../shared/api/session';
import { CalendarDate } from '../../../shared/dates/CalendarDate';
import { AutoLoadMore } from '../../../shared/ui/AutoLoadMore';
import { useColumnFilters } from '../../registry/RegistryColumnFilters';
import {
  filterRequest,
  restoreColumnFilters,
  type ColumnFilter,
} from '../../registry/registryFilters';
import { preferenceKey, useRegistryPreference } from '../../registry/registryPreferences';
import {
  RegistryTable,
  RegistryColumnSettings,
  useRegistryColumns,
  type RegistryColumn,
} from '../../registry/RegistryTable';
import { formatFixed, type ServiceCharge } from './serviceCharges';
import type { ServiceChargeInvoice } from '../invoices/invoices';
import { serviceChargeRow } from './serviceChargeRow';
import { financialLabelUnits, vatLabel } from '../labels/financialLabels';

type Cursor = { id: string; scope: string; text_value?: string; date_value?: string };
type Page = { items: ServiceCharge[]; invoices: ServiceChargeInvoice[]; next_cursor?: Cursor };
type Row = ServiceCharge & { invoice?: ServiceChargeInvoice };
const rows = (page: Page): Row[] =>
  page.items.map((item) => ({
    ...item,
    invoice: page.invoices.find((i) => i.service_charge_id === item.id),
  }));
const initial = {
  filters: [{ field: 'archived', values: ['false'] }] as ColumnFilter[],
  sort: 'service_date',
  descending: true,
};
function restore(raw: unknown): typeof initial {
  const value = raw && typeof raw === 'object' ? (raw as Partial<typeof initial>) : {};
  return {
    filters: restoreColumnFilters(value.filters) ?? initial.filters,
    sort: typeof value.sort === 'string' && value.sort.length <= 64 ? value.sort : initial.sort,
    descending: typeof value.descending === 'boolean' ? value.descending : initial.descending,
  };
}
export function ServiceChargeRegistry({
  session,
  version,
  onEdit,
  onCreate,
}: {
  session: Session;
  version: number;
  onEdit: (charge: ServiceCharge, invoice?: ServiceChargeInvoice) => void;
  onCreate: (direction: 'income' | 'expense') => void;
}) {
  const [items, setItems] = useState<Row[]>([]);
  const [references, setReferences] = useState<Record<string, string>>({});
  const referenceIds = useMemo(
    () => ({
      shipments: [
        ...new Set(items.flatMap((item) => (item.shipment_id ? [item.shipment_id] : []))),
      ],
      transportations: [
        ...new Set(
          items.flatMap((item) => (item.transportation_id ? [item.transportation_id] : [])),
        ),
      ],
    }),
    [items],
  );
  useEffect(() => {
    const controller = new AbortController();
    setReferences({});
    async function load(source: 'shipments' | 'transportations', ids: string[]) {
      if (!session.permissions.includes(`${source}.read`)) return;
      // Загружаем ссылки пакетами, а не отдельным запросом на каждую услугу.
      for (let offset = 0; offset < ids.length; offset += 100) {
        const page = await request<{
          items: { id: string; number?: string; container_number?: string | null }[];
        }>(
          `/api/v1/${source}/list`,
          session,
          { filters: [{ field: 'id', values: ids.slice(offset, offset + 100) }], limit: 100 },
          controller.signal,
        );
        if (controller.signal.aborted) return;
        setReferences((old) => ({
          ...old,
          ...Object.fromEntries(
            page.items.map((item) => [
              item.id,
              source === 'shipments'
                ? (item.number ?? 'Без номера')
                : item.container_number || 'Без номера',
            ]),
          ),
        }));
      }
    }
    for (const source of ['shipments', 'transportations'] as const) {
      void load(source, referenceIds[source]).catch(() => {
        // Недоступность связанной карточки не должна скрывать саму услугу.
      });
    }
    return () => controller.abort();
  }, [referenceIds, session]);
  const [cursor, setCursor] = useState<Cursor>();
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState('');
  const [refresh, setRefresh] = useState(0);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, 'service-charges', 'query'),
    initial,
    restore,
  );
  const filters = useColumnFilters({
    registry: 'service-charges',
    session,
    query,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  useEffect(() => {
    const controller = new AbortController();
    setLoading(true);
    setFailure('');
    setItems([]);
    setCursor(undefined);
    request<Page>(
      '/api/v1/service-charges/registry',
      session,
      { ...query, filters: filterRequest(query.filters), limit: 50 },
      controller.signal,
    )
      .then((page) => {
        if (!controller.signal.aborted) {
          setItems(rows(page));
          setCursor(page.next_cursor);
        }
      })
      .catch((error) => {
        if (!controller.signal.aborted) setFailure(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLoading(false);
      });
    return () => controller.abort();
  }, [query, session, version, refresh]);
  async function more() {
    if (!cursor || loading) return;
    setLoading(true);
    setFailure('');
    try {
      const page = await request<Page>('/api/v1/service-charges/registry', session, {
        ...query,
        filters: filterRequest(query.filters),
        limit: 50,
        cursor,
      });
      setItems((old) => [...old, ...rows(page)]);
      setCursor(page.next_cursor);
    } catch (error) {
      setFailure((error as Error).message);
    } finally {
      setLoading(false);
    }
  }
  const write = session.permissions.includes('finance.write_service_charges');
  const columns: RegistryColumn<Row>[] = [
    {
      id: 'label_name',
      label: 'Услуга',
      width: 190,
      required: true,
      sort: 'label_name',
      render: (item) => <strong>{item.label_name}</strong>,
    },
    {
      id: 'counterparty_id',
      label: 'Контрагент',
      width: 200,
      render: (item) => item.counterparty_name,
    },
    {
      id: 'direction',
      label: 'Направление',
      width: 160,
      render: (item) => (item.direction === 'income' ? 'Доход' : 'Расход'),
    },
    {
      id: 'service_date',
      label: 'Дата услуги',
      width: 150,
      render: (item) => CalendarDate.fromISO(item.service_date).toDisplayString(),
    },
    {
      id: 'quantity',
      defaultVisible: false,
      label: 'Количество × цена',
      width: 240,
      filterFields: [],
      render: (item) => (
        <>
          {formatFixed(item.quantity_milli, 3)} {financialLabelUnits[item.unit]}
          <span className="service-secondary">
            × {formatFixed(item.price_minor, 2)} {item.currency}
          </span>
        </>
      ),
    },
    {
      id: 'net_minor',
      defaultVisible: false,
      label: 'Без НДС',
      width: 170,
      sort: 'net_minor',
      filterFields: ['currency'],
      render: (item) => `${formatFixed(item.net_minor, 2)} ${item.currency}`,
    },
    {
      id: 'vat_minor',
      defaultVisible: false,
      label: 'НДС',
      width: 140,
      sort: 'vat_minor',
      filterFields: [],
      render: (item) => (
        <>
          {formatFixed(item.vat_minor, 2)}
          <span className="service-secondary">{vatLabel(item)}</span>
        </>
      ),
    },
    {
      id: 'total_minor',
      label: 'Итого',
      width: 150,
      sort: 'total_minor',
      filterFields: ['currency'],
      render: (item) => (
        <strong>
          {formatFixed(item.total_minor, 2)} {item.currency}
        </strong>
      ),
    },
    {
      id: 'invoice_number',
      label: 'Счёт',
      width: 200,
      render: (item) =>
        item.invoice ? (
          <a
            className="link"
            href={`#/${item.invoice.direction}-invoices/${item.invoice.invoice_id}`}
            onClick={(e) => e.stopPropagation()}
          >
            № {item.invoice.number}
            <span className="service-secondary">
              от {CalendarDate.fromISO(item.invoice.invoice_date).toDisplayString()}
            </span>
          </a>
        ) : (
          'Не выбран'
        ),
    },
    {
      id: 'shipment_id',
      defaultVisible: false,
      label: 'Поставка',
      width: 170,
      render: (item) =>
        item.shipment_id ? (
          <a
            className="link"
            href={`#/shipments/${item.shipment_id}`}
            onClick={(e) => e.stopPropagation()}
          >
            {references[item.shipment_id] ?? 'Поставка'}
          </a>
        ) : (
          '—'
        ),
    },
    {
      id: 'transportation_id',
      defaultVisible: false,
      label: 'Перевозка',
      width: 170,
      render: (item) =>
        item.transportation_id ? (
          <a
            className="link"
            href={`#/transportations/${item.transportation_id}`}
            onClick={(e) => e.stopPropagation()}
          >
            {references[item.transportation_id] ?? 'Перевозка'}
          </a>
        ) : (
          '—'
        ),
    },
    {
      id: 'archived',
      defaultVisible: false,
      label: 'Состояние',
      width: 160,
      render: (item) => (item.archived ? 'В архиве' : 'Действующая'),
    },
  ];
  const state = useRegistryColumns(session.subject, 'service-charges', columns);
  return (
    <section className="service-charge-registry">
      <RegistryHeading
        title={'Услуги'}
        count={items.length}
        onRefresh={() => setRefresh((v) => v + 1)}
      >
        {write && (
          <>
            <button className="primary" onClick={() => onCreate('income')}>
              Добавить доходную услугу
            </button>
            <button onClick={() => onCreate('expense')}>Добавить расходную услугу</button>
          </>
        )}
        <RegistryColumnSettings {...state} />
      </RegistryHeading>
      {failure && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <RegistryTable
        items={items}
        columns={state}
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
        onOpen={(item) => onEdit(item, item.invoice)}
        rowAttributes={(item) =>
          serviceChargeRow(
            item,
            () => onEdit(item, item.invoice),
            !write || item.invoice?.read_only === true,
          )
        }
        emptyMessage={loading ? 'Загружаем услуги…' : failure ? undefined : 'Услуги не найдены'}
      />
      <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
    </section>
  );
}

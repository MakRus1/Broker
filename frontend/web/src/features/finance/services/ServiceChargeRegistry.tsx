import { useNotifications } from '../../notifications/Notifications';
import { useRegistryPage } from '../../registry/useRegistryPage';
import { RegistryResultState } from '../../../shared/ui/RegistryExperience';
import { registryValueLabel } from '../../registry/registryLabels';
import { useLocalization } from '../../localization/Localization';
import { RegistrySelectionActions } from '../../registry/RegistrySelectionActions';
import { usePagedRowSelection } from '../../registry/usePagedRowSelection';
import { ServiceInvoiceActions } from '../invoices/ServiceInvoiceActions';
import { attachBulkInvoice, bulkInvoiceProblem } from '../invoices/bulkInvoices';
import './bulkServices.css';
import { RegistryHeading } from '../../../shared/ui/RegistryHeading';
import { useCallback, useEffect, useMemo, useState } from 'react';
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
  useRegistryColumns,
  type RegistryColumn,
} from '../../registry/RegistryTable';
import { formatFixed, type ServiceCharge } from './serviceCharges';
import type { ServiceChargeInvoice } from '../invoices/invoices';
import { serviceChargeRow } from './serviceChargeRow';
import { vatLabel } from '../labels/financialLabels';

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
  active = true,
  session,
  version,
  onEdit,
  onCreate,
}: {
  active?: boolean;
  session: Session;
  version: number;
  onEdit: (charge: ServiceCharge, invoice?: ServiceChargeInvoice) => void;
  onCreate: (direction: 'income' | 'expense') => void;
}) {
  const { translate, locale } = useLocalization();
  const [failure, setFailure] = useState('');
  const [refresh, setRefresh] = useState(0);
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, 'service-charges', 'query'),
    initial,
    restore,
  );
  const loadPage = useCallback(
    async (cursor: Cursor | undefined, signal: AbortSignal) => {
      setFailure('');
      const result = await request<Page>(
        '/api/v1/service-charges/registry',
        session,
        { ...query, filters: filterRequest(query.filters), limit: 50, cursor },
        signal,
      );
      return { items: rows(result), next_cursor: result.next_cursor };
    },
    [query, session, version],
  );
  const page = useRegistryPage({
    active,
    refreshVersion: refresh,
    loadPage,
    onError: (error) => setFailure((error as Error).message),
  });
  const items = page.items;
  const loading = page.loading;
  const cursor = page.more;
  const more = page.loadMore;
  const selection = usePagedRowSelection(page, 500, active);
  const [menu, setMenu] = useState<{ x: number; y: number; ids: string[] }>();
  const [bulk, setBulk] = useState<{ mode: 'new' | 'existing'; items: Row[] }>();
  const notifications = useNotifications();
  const setNotice = (message: string) =>
    notifications.publish({ level: 'success', message, route: '/service-charges' });
  const selectedItems = selection.items;
  const canInvoice = session.permissions.includes('finance.write_invoices');
  const problem =
    selection.selected.size > 500
      ? translate('bulk.chargeLimit')
      : selection.resolving
        ? translate('ui.loadingRecords')
        : selection.error || bulkInvoiceProblem(selectedItems, locale);
  function openBulk(mode: 'new' | 'existing', chosen = selectedItems) {
    if (bulkInvoiceProblem(chosen, locale)) return;
    setMenu(undefined);
    setBulk({ mode, items: chosen });
  }
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
                ? (item.number ?? translate('transport.noNumber'))
                : item.container_number || translate('transport.noNumber'),
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
  useEffect(() => {
    selection.clear();
    setMenu(undefined);
  }, [query, session, version, refresh]);
  const filters = useColumnFilters({
    registry: 'service-charges',
    session,
    query,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  const write = session.permissions.includes('finance.write_service_charges');
  const columns: RegistryColumn<Row>[] = [
    {
      id: 'label_name',
      labelKey: 'entity.serviceCharge',
      width: 190,
      required: true,
      sort: 'label_name',
      render: (item) => <strong>{item.label_name}</strong>,
    },
    {
      id: 'counterparty_id',
      labelKey: 'registryField.counterparty_id',
      width: 200,
      render: (item) => item.counterparty_name,
    },
    {
      id: 'direction',
      labelKey: 'registryField.direction',
      width: 160,
      render: (item) => registryValueLabel('direction', item.direction, locale),
    },
    {
      id: 'service_date',
      labelKey: 'registryField.service_date',
      width: 150,
      render: (item) => CalendarDate.fromISO(item.service_date).toDisplayString(),
    },
    {
      id: 'quantity',
      defaultVisible: false,
      labelKey: 'registryField.quantityPrice',
      width: 240,
      filterFields: [],
      render: (item) => (
        <>
          {formatFixed(item.quantity_milli, 3)} {registryValueLabel('unit', item.unit, locale)}
          <span className="service-secondary">
            × {formatFixed(item.price_minor, 2)} {item.currency}
          </span>
        </>
      ),
    },
    {
      id: 'net_minor',
      defaultVisible: false,
      labelKey: 'ui.noVat',
      width: 170,
      sort: 'net_minor',
      filterFields: ['currency'],
      render: (item) => `${formatFixed(item.net_minor, 2)} ${item.currency}`,
    },
    {
      id: 'vat_minor',
      defaultVisible: false,
      labelKey: 'registryField.vat_minor',
      width: 140,
      sort: 'vat_minor',
      filterFields: [],
      render: (item) => (
        <>
          {formatFixed(item.vat_minor, 2)}
          <span className="service-secondary">{vatLabel(item, locale)}</span>
        </>
      ),
    },
    {
      id: 'total_minor',
      labelKey: 'registryField.total_minor',
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
      labelKey: 'registryField.invoice_number',
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
              {translate('ui.datePrefix')}{' '}
              {CalendarDate.fromISO(item.invoice.invoice_date).toDisplayString()}
            </span>
          </a>
        ) : (
          translate('ui.notChosen')
        ),
    },
    {
      id: 'shipment_id',
      defaultVisible: false,
      labelKey: 'registryField.shipment_id',
      width: 170,
      render: (item) =>
        item.shipment_id ? (
          <a
            className="link"
            href={`#/shipments/${item.shipment_id}`}
            onClick={(e) => e.stopPropagation()}
          >
            {references[item.shipment_id] ?? translate('registryField.shipment_id')}
          </a>
        ) : (
          '—'
        ),
    },
    {
      id: 'transportation_id',
      defaultVisible: false,
      labelKey: 'registryField.transportation_id',
      width: 170,
      render: (item) =>
        item.transportation_id ? (
          <a
            className="link"
            href={`#/transportations/${item.transportation_id}`}
            onClick={(e) => e.stopPropagation()}
          >
            {references[item.transportation_id] ?? translate('registryField.transportation_id')}
          </a>
        ) : (
          '—'
        ),
    },
    {
      id: 'archived',
      defaultVisible: false,
      labelKey: 'registryField.archived',
      width: 160,
      render: (item) => (item.archived ? translate('filter.archived') : translate('filter.active')),
    },
  ];
  const state = useRegistryColumns(session.subject, 'service-charges', columns, {
    query: query,
    apply: (raw) => setQuery(restore(raw)),
  });
  return (
    <section className="service-charge-registry registry-page registry-workbench">
      <RegistryHeading
        columns={state}
        title={translate('nav.serviceCharges')}
        count={page.count}
        loading={loading}
        onRefresh={() => setRefresh((v) => v + 1)}
      >
        {write && (
          <>
            <button className="primary" onClick={() => onCreate('income')}>
              {translate('registryPage.addIncome')}
            </button>
            <button onClick={() => onCreate('expense')}>
              {translate('registryPage.addExpense')}
            </button>
          </>
        )}
      </RegistryHeading>
      {canInvoice && (
        <RegistrySelectionActions
          count={selection.selected.size}
          problem={problem}
          actions={[
            { label: translate('ui.createInvoice'), onClick: () => openBulk('new') },
            { label: translate('ui.addToInvoice'), onClick: () => openBulk('existing') },
          ]}
          onClear={selection.clear}
          menu={menu}
          onCloseMenu={() => setMenu(undefined)}
        />
      )}
      {bulk && (
        <ServiceInvoiceActions
          service={bulk.items[0]}
          session={session}
          initialMode={bulk.mode}
          onClose={() => setBulk(undefined)}
          summary={
            <p className="hint">
              {translate('bulk.chargeCount', { count: bulk.items.length })} ·{' '}
              {bulk.items[0].counterparty_name} · {translate('registryField.total_minor')}:{' '}
              {formatFixed(
                bulk.items.reduce((sum, item) => sum + BigInt(item.invoice_total_minor), 0n),
                2,
              )}{' '}
              {bulk.items[0].invoice_currency}
            </p>
          }
          onSelect={async (value) => {
            if (!value) return;
            const saved = await attachBulkInvoice(
              bulk.items.map((item) => item.id),
              value,
              session,
            );
            setNotice(
              translate('bulk.invoiceSaved', {
                number: saved.invoice.number,
                count: bulk.items.length,
              }),
            );
            selection.clear();
            setRefresh((old) => old + 1);
          }}
        />
      )}
      {failure && items.length > 0 && (
        <p className="error" role="alert">
          {failure}
        </p>
      )}
      <RegistryTable
        pageSource={page.source}
        selectionColumn={
          canInvoice
            ? {
                header: (
                  <input
                    type="checkbox"
                    aria-label={translate('ui.selectCharges')}
                    ref={(node) => {
                      if (node)
                        node.indeterminate =
                          selection.selected.size > 0 && selection.selected.size < page.count;
                    }}
                    checked={selection.allSelected}
                    onChange={(event) =>
                      selection.setSelected(event.target.checked ? new Set(page.ids) : new Set())
                    }
                  />
                ),
                render: (item) => (
                  <input
                    type="checkbox"
                    aria-label={translate('ui.selectCharge', { name: item.label_name })}
                    checked={selection.selected.has(item.id)}
                    onPointerDown={(event) => selection.start(event, item.id)}
                    onClick={(event) => {
                      event.stopPropagation();
                      if (
                        event.detail === 0 ||
                        (event.nativeEvent as PointerEvent).pointerType === 'touch'
                      )
                        selection.toggle(item.id, !selection.selected.has(item.id), event.shiftKey);
                    }}
                    onChange={() => {}}
                  />
                ),
              }
            : undefined
        }
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
        rowAttributes={(item) => ({
          ...serviceChargeRow(
            item,
            () => onEdit(item, item.invoice),
            !write || item.invoice?.read_only === true,
            locale,
          ),
          'data-selection-row': item.id,
          'aria-selected': selection.selected.has(item.id),
          onContextMenu: canInvoice
            ? (event) => {
                if ((event.target as HTMLElement).closest('a,input,button')) return;
                event.preventDefault();
                const ids = selection.selected.has(item.id) ? [...selection.selected] : [item.id];
                if (!selection.selected.has(item.id)) selection.setSelected(new Set(ids));
                setMenu({
                  x: Math.max(8, Math.min(event.clientX, innerWidth - 320)),
                  y: Math.max(8, Math.min(event.clientY, innerHeight - 240)),
                  ids,
                });
              }
            : undefined,
        })}
        emptyMessage={
          <RegistryResultState
            loading={loading}
            failed={Boolean(failure)}
            failureMessage={failure}
            filtered={Boolean(query.filters?.length)}
            title={translate('registryPage.noCharges')}
            onRetry={() => {
              setRefresh((value) => value + 1);
            }}
            onReset={() => setQuery((old) => ({ ...old, filters: [] }))}
            create={
              write
                ? { label: translate('registryPage.addIncome'), onClick: () => onCreate('income') }
                : undefined
            }
          />
        }
      />
      <AutoLoadMore enabled={!!cursor} loading={loading} failed={!!failure} onLoad={more} />
    </section>
  );
}

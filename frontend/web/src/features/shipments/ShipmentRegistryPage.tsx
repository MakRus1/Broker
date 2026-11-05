import { RegistryEmptyState } from '../../shared/ui/RegistryExperience';
import { useCallback, useEffect } from 'react';
import { RegistryHeading } from '../../shared/ui/RegistryHeading';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { CalendarDate } from '../../shared/dates/CalendarDate';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import type { Counterparty } from '../counterparties/CustomerSelect';
import { useLocalization } from '../localization/Localization';
import { useColumnFilters } from '../registry/RegistryColumnFilters';
import { RegistryTable, useRegistryColumns, type RegistryColumn } from '../registry/RegistryTable';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { restoreShipmentQuery } from '../registry/registryQueryPreferences';
import { useRegistryPage } from '../registry/useRegistryPage';
import {
  initialShipmentQuery,
  shipmentRegistryRequest,
  type ShipmentRegistryCursor,
} from './shipmentRegistry';
import type { Shipment } from './types';

export function ShipmentRegistry({
  session,
  active,
  refreshVersion,
  customers,
  onCustomersLoaded,
  onError,
}: {
  session: Session;
  active: boolean;
  refreshVersion: number;
  customers: Counterparty[];
  onCustomersLoaded: (items: Counterparty[]) => void;
  onError: (error: unknown) => void;
}) {
  const { translate } = useLocalization();
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, 'shipments', 'query'),
    initialShipmentQuery,
    restoreShipmentQuery,
  );
  const loadPage = useCallback(
    (cursor: ShipmentRegistryCursor | undefined, signal: AbortSignal) =>
      request<{ items: Shipment[]; next_cursor?: ShipmentRegistryCursor }>(
        '/api/v1/shipments/list',
        session,
        shipmentRegistryRequest(query, cursor),
        signal,
      ),
    [session, query],
  );
  const page = useRegistryPage({
    active: active && session.permissions.includes('shipments.read'),
    refreshVersion,
    loadPage,
    onError,
  });
  const filters = useColumnFilters({
    registry: 'shipments',
    session,
    query,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  useEffect(() => {
    if (!session.permissions.includes('counterparties.read')) return;
    const ids = [...new Set(page.items.map((item) => item.customer_id))]
      .filter((id) => !customers.some((item) => item.id === id))
      .slice(0, 100);
    if (!ids.length) return;
    const controller = new AbortController();
    void request<{ items: Counterparty[] }>(
      '/api/v1/counterparties/list',
      session,
      { filters: [{ field: 'id', values: ids }], limit: 100 },
      controller.signal,
    )
      .then((result) => {
        if (!controller.signal.aborted && result.items.length) onCustomersLoaded(result.items);
      })
      .catch((reason: unknown) => {
        if (!controller.signal.aborted) onError(reason);
      });
    return () => controller.abort();
  }, [session, page.items, customers, onCustomersLoaded, onError]);
  const customerName = (id: string) =>
    customers.find((item) => item.id === id)?.name || translate('lookup.counterpartyUnavailable');
  const canCreate = session.permissions.includes('shipments.write');
  const hasFilters = Boolean(query.filters?.length);
  const createShipment = () => {
    location.hash = '/shipments/new/' + crypto.randomUUID();
  };
  const columns: RegistryColumn<Shipment>[] = [
    {
      id: 'number',
      labelKey: 'field.shipment',
      width: 180,
      required: true,
      sort: 'number',
      render: (item) => (
        <a
          className="link"
          href={`#/shipments/${item.id}`}
          onClick={(event) => event.stopPropagation()}
        >
          {item.number}
        </a>
      ),
    },
    {
      id: 'shipment_date',
      labelKey: 'field.shipmentDate',
      width: 190,
      sort: 'shipment_date',
      render: (item) =>
        item.shipment_date ? CalendarDate.fromISO(item.shipment_date).toDisplayString() : '—',
    },
    {
      id: 'customer',
      filterFields: ['customer_id'],
      labelKey: 'field.customer',
      width: 280,
      render: (item) => customerName(item.customer_id),
    },
    {
      id: 'cargo_description',
      labelKey: 'field.cargo',
      width: 340,
      sort: 'cargo_description',
      render: (item) => item.cargo_description || '—',
    },
    {
      id: 'status',
      labelKey: 'field.status',
      width: 180,
      sort: 'status',
      render: (item) => <span className="badge">{item.status || '—'}</span>,
    },
  ];

  const columnState = useRegistryColumns(session.subject, 'shipments', columns, {
    active,
    query: query,
    apply: (raw) => setQuery(restoreShipmentQuery(raw)),
  });
  return (
    <div className="registry-page shipment-registry registry-workbench" hidden={!active}>
      <RegistryHeading
        columns={columnState}
        title={translate('nav.shipments')}
        count={page.count}
        loading={page.loading}
        onRefresh={() => void page.refresh()}
      >
        {canCreate && (
          <button className="primary" onClick={createShipment}>
            {translate('action.createShipment')}
          </button>
        )}
      </RegistryHeading>
      <section className="surface registry-workbench-surface" aria-busy={page.loading}>
        <RegistryTable
          items={page.items}
          pageSource={page.source}
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
            location.hash = `/shipments/${item.id}`;
          }}
        />
        {!page.loading && !page.items.length && (
          <RegistryEmptyState
            kind={page.failed ? 'failure' : hasFilters ? 'filtered' : 'empty'}
            title={translate(
              page.failed
                ? 'registry.loadFailed'
                : hasFilters
                  ? 'registry.noMatches'
                  : canCreate
                    ? 'registry.firstShipment'
                    : 'empty.shipments',
            )}
            description={translate(
              page.failed
                ? 'registry.loadFailedHint'
                : hasFilters
                  ? 'registry.noMatchesHint'
                  : 'registry.firstShipmentHint',
            )}
            action={
              page.failed
                ? { label: translate('filter.retry'), onClick: () => void page.refresh() }
                : hasFilters
                  ? {
                      label: translate('filter.reset'),
                      onClick: () => setQuery((old) => ({ ...old, filters: [] })),
                    }
                  : canCreate
                    ? { label: translate('action.createShipment'), onClick: createShipment }
                    : undefined
            }
          />
        )}
        <AutoLoadMore
          enabled={active && page.more}
          loading={page.loading}
          failed={page.failed}
          onLoad={page.loadMore}
        />
      </section>
    </div>
  );
}

import { RegistryPage } from '../../shared/ui/RegistryPage';
import { RegistryResultState } from '../../shared/ui/RegistryExperience';
import { useCallback } from 'react';
import { RegistryHeading } from '../../shared/ui/RegistryHeading';
import { AutoLoadMore } from '../../shared/ui/AutoLoadMore';
import { request } from '../../shared/api/http';
import type { Session } from '../../shared/api/session';
import { useLocalization } from '../localization/Localization';
import { useColumnFilters } from '../registry/RegistryColumnFilters';
import { useRegistryColumns } from '../registry/RegistryTable';
import { preferenceKey, useRegistryPreference } from '../registry/registryPreferences';
import { restoreTransportationQuery } from '../registry/registryQueryPreferences';
import { useRegistryPage } from '../registry/useRegistryPage';
import { useRouteRegistryFilters } from '../routes/RouteRegistryFilters';
import { TransportationTable, transportationColumns } from './TransportationTable';
import {
  initialRegistryQuery,
  registryRequest,
  type RegistryCursor,
} from './transportationRegistry';
import type { Transportation } from './types';

export function TransportationRegistry({
  session,
  active,
  refreshVersion,
  onError,
}: {
  session: Session;
  active: boolean;
  refreshVersion: number;
  onError: (error: unknown) => void;
}) {
  const { translate } = useLocalization();
  const [query, setQuery] = useRegistryPreference(
    preferenceKey(session.subject, 'transportations', 'query'),
    initialRegistryQuery,
    restoreTransportationQuery,
  );
  const loadPage = useCallback(
    async (cursor: RegistryCursor | undefined, signal: AbortSignal) => {
      // Web BFF сам подставляет номер поставки и клиента, учитывая права пользователя.
      const page = await request<{
        items: Transportation[];
        next_cursor?: RegistryCursor;
        shipments?: { id: string; number: string; customer_name?: string }[];
        unavailable?: ('shipments' | 'customers')[];
      }>('/api/v1/transportations/list', session, registryRequest(query, cursor), signal);
      const shipmentById = new Map((page.shipments ?? []).map((item) => [item.id, item]));
      const shipmentsUnavailable = page.unavailable?.includes('shipments') ?? false;
      const customersUnavailable = page.unavailable?.includes('customers') ?? false;
      return {
        next_cursor: page.next_cursor,
        items: page.items.map((item) => {
          const shipment = shipmentById.get(item.shipment_id);
          return {
            ...item,
            shipment_number: shipment?.number,
            customer_name: shipment?.customer_name,
            shipment_unavailable: shipmentsUnavailable,
            customer_unavailable:
              shipmentsUnavailable || (Boolean(shipment) && customersUnavailable),
          };
        }),
      };
    },
    [session, query],
  );
  const page = useRegistryPage({
    active: active && session.permissions.includes('transportations.read'),
    refreshVersion,
    loadPage,
    onError,
  });
  const filters = useColumnFilters({
    registry: 'transportations',
    session,
    query,
    onChange: (filters) => setQuery((old) => ({ ...old, filters })),
    onSort: (sort, descending) => setQuery((old) => ({ ...old, sort, descending })),
  });
  const routeFilters = useRouteRegistryFilters(session, query, (route_filters) =>
    setQuery((old) => ({ ...old, route_filters })),
  );
  const columns = useRegistryColumns(session.subject, 'transportations', transportationColumns, {
    active,
    query: query,
    apply: (raw) => setQuery(restoreTransportationQuery(raw)),
  });
  return (
    <RegistryPage
      className="transportation-registry"
      hidden={!active}
      heading={
        <RegistryHeading
          columns={columns}
          title={translate('nav.transportations')}
          count={page.count}
          loading={page.loading}
          onRefresh={() => void page.refresh()}
        />
      }
    >
      <section className="surface registry-workbench-surface" aria-busy={page.loading}>
        {routeFilters.editor}
        {page.items.some((item) => item.shipment_unavailable || item.customer_unavailable) && (
          <p className="route-notice" role="status">
            {translate('transportations.referencesUnavailable')}
          </p>
        )}
        <TransportationTable
          routeFilter={routeFilters.button}
          routeFilterSummary={routeFilters.toolbar}
          items={page.items}
          pageSource={page.source}
          filters={filters}
          columns={columns}
          sort={query.sort}
          descending={query.descending}
          onSort={(sort) =>
            setQuery((old) => ({
              ...old,
              sort,
              descending: old.sort === sort ? !old.descending : false,
            }))
          }
        />
        {!page.items.length && (
          <RegistryResultState
            loading={page.loading}
            failed={page.failed}
            filtered={Boolean(query.filters?.length || query.route_filters?.length)}
            title={translate('empty.transportations')}
            onRetry={() => void page.refresh()}
            onReset={() => setQuery((old) => ({ ...old, filters: [], route_filters: [] }))}
          />
        )}
        <AutoLoadMore
          enabled={active && page.more}
          loading={page.loading}
          failed={page.failed}
          onLoad={page.loadMore}
        />
      </section>
    </RegistryPage>
  );
}

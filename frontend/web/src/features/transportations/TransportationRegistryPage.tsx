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
import { useRouteRegistryFilters } from './RouteRegistryFilters';
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
    (cursor: RegistryCursor | undefined, signal: AbortSignal) =>
      request<{ items: Transportation[]; next_cursor?: RegistryCursor }>(
        '/api/v1/transportations/list',
        session,
        registryRequest(query, cursor),
        signal,
      ),
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
  const columns = useRegistryColumns(session.subject, 'transportations', transportationColumns);
  return (
    <div className="registry-page transportation-registry" hidden={!active}>
      <RegistryHeading
        columns={columns}
        title={translate('nav.transportations')}
        count={page.items.length}
        loading={page.loading}
        onRefresh={() => void page.refresh()}
      />
      <section className="surface" aria-busy={page.loading}>
        {routeFilters.editor}
        <TransportationTable
          routeFilter={routeFilters.button}
          routeFilterSummary={routeFilters.toolbar}
          items={page.items}
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
        {!page.loading && !page.items.length && (
          <div className="empty">
            <span>▣</span>
            <h2>{translate('empty.transportations')}</h2>
            <p>{translate('transport.emptyRegistry')}</p>
          </div>
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

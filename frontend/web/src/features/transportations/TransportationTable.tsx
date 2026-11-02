import { useLocalization } from '../localization/Localization';
import { registryValueLabel } from '../registry/registryLabels';
import type { ColumnFilterController } from '../registry/RegistryColumnFilters';
import {
  RegistryTable,
  useRegistryColumns,
  type RegistryColumns,
  type RegistryColumn,
} from '../registry/RegistryTable';
import type { RegistrySort } from './transportationRegistry';
import { routePoint } from './transportation';
import type { ReactNode } from 'react';
import { ownership } from '../routes/logisticsLabels';
import { type Transportation } from './types';
export const transportationColumns: RegistryColumn<Transportation>[] = [
  {
    id: 'container_number',
    labelKey: 'entity.transportation',
    filterFields: ['container_number', 'type', 'hazardous', 'shipment_id'],
    cellClassName: 'transportation-number-cell',
    width: 250,
    required: true,
    sort: 'container_number',
    render: (item) => <TransportationIdentity item={item} />,
  },
  {
    id: 'container_type',
    labelKey: 'registryField.container',
    filterFields: ['container_type', 'container_ownership'],
    cellClassName: 'transportation-container-cell',
    width: 180,
    sort: 'container_type',
    render: (item) => (
      <>
        {item.container_type_code ? (
          <strong>{item.container_type_code}</strong>
        ) : (
          <span className="registry-muted">—</span>
        )}
        {item.container_ownership && (
          <span className="registry-ownership">{ownership[item.container_ownership]}</span>
        )}
      </>
    ),
  },
  {
    id: 'route',
    labelKey: 'routeFilter.route',
    filterFields: false,
    width: 480,
    render: (item) => <RouteRegistrySummary item={item} />,
  },
];

function RouteRegistrySummary({ item }: { item: Transportation }) {
  const { translate, locale } = useLocalization();
  return (
    <>
      {item.route_segments.length ? (
        <div className="registry-route">
          {item.route_segments.slice(0, 2).map((stage) => (
            <div className="registry-route-stage" key={stage.id}>
              <span className="registry-route-mode">
                {registryValueLabel('mode', stage.mode, locale)}
              </span>
              {(routePoint(stage, 'origin').name || routePoint(stage, 'destination').name) && (
                <span>
                  {routePoint(stage, 'origin').name || translate('routeFilter.pointNotSet')} →{' '}
                  {routePoint(stage, 'destination').name || translate('routeFilter.pointNotSet')}
                </span>
              )}
            </div>
          ))}
          {item.route_segments.length > 2 && (
            <span className="registry-secondary">
              {translate('routeFilter.more', { count: item.route_segments.length - 2 })}
            </span>
          )}
        </div>
      ) : (
        <span className="registry-muted">{translate('routeFilter.notSet')}</span>
      )}
    </>
  );
}

export function TransportationTable({
  items,
  sort,
  descending = false,
  onSort,
  columns,
  filters,
  routeFilter,
  routeFilterSummary,
}: {
  routeFilterSummary?: ReactNode;
  filters?: ColumnFilterController;
  routeFilter?: { active: boolean; expanded?: boolean; onOpen: (anchor: HTMLElement) => void };
  items: Transportation[];
  sort?: RegistrySort;
  descending?: boolean;
  onSort?: (sort: RegistrySort) => void;
  columns?: RegistryColumns;
}) {
  const fallback = useRegistryColumns('', 'transportations', transportationColumns);
  return (
    <RegistryTable
      items={items}
      filters={filters}
      customFilters={routeFilter ? { route: routeFilter } : undefined}
      filterSummary={routeFilterSummary}
      columns={columns ?? fallback}
      definitions={transportationColumns}
      sort={sort}
      descending={descending}
      onSort={onSort ? (field) => onSort(field as RegistrySort) : undefined}
      onOpen={(item) => {
        location.hash = `/transportations/${item.id}`;
      }}
    />
  );
}

function TransportationIdentity({ item }: { item: Transportation }) {
  const { translate, locale } = useLocalization();
  return (
    <>
      <a
        className="link"
        href={`#/transportations/${item.id}`}
        title={item.container_number || translate('filter.noNumber')}
        onClick={(event) => event.stopPropagation()}
      >
        {item.container_number || translate('filter.noNumber')}
      </a>
      <span className="registry-secondary">{registryValueLabel('type', item.type, locale)}</span>
      {item.hazardous && (
        <span className="registry-hazard">{translate('registryField.hazardous')}</span>
      )}
    </>
  );
}

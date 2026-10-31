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
import { modes, ownership, transportationTypes } from '../routes/logisticsLabels';
import { type Transportation } from './types';
export const transportationColumns: RegistryColumn<Transportation>[] = [
  {
    id: 'container_number',
    filterFields: ['container_number', 'type', 'hazardous', 'shipment_id'],
    cellClassName: 'transportation-number-cell',
    label: 'Перевозка',
    width: 250,
    required: true,
    sort: 'container_number',
    render: (item) => (
      <>
        <a
          className="link"
          href={`#/transportations/${item.id}`}
          title={item.container_number || 'Без номера'}
          onClick={(event) => event.stopPropagation()}
        >
          {item.container_number || 'Без номера'}
        </a>
        <span className="registry-secondary">{transportationTypes[item.type]}</span>
        {item.hazardous && <span className="registry-hazard">Опасный груз</span>}
      </>
    ),
  },
  {
    id: 'container_type',
    filterFields: ['container_type', 'container_ownership'],
    cellClassName: 'transportation-container-cell',
    label: 'Контейнер',
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
    filterFields: false,
    label: 'Маршрут',
    width: 480,
    render: (item) => (
      <>
        {item.route_segments.length ? (
          <div className="registry-route">
            {item.route_segments.slice(0, 2).map((stage) => (
              <div className="registry-route-stage" key={stage.id}>
                <span className="registry-route-mode">{modes[stage.mode]}</span>
                {(routePoint(stage, 'origin').name || routePoint(stage, 'destination').name) && (
                  <span>
                    {routePoint(stage, 'origin').name || 'Не указан'} →{' '}
                    {routePoint(stage, 'destination').name || 'Не указан'}
                  </span>
                )}
              </div>
            ))}
            {item.route_segments.length > 2 && (
              <span className="registry-secondary">
                Ещё участков: {item.route_segments.length - 2}
              </span>
            )}
          </div>
        ) : (
          <span className="registry-muted">Не задан</span>
        )}
      </>
    ),
  },
];
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

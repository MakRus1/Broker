import type { RegistryPageSource } from '../registry/registryPageCache';
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
import { calendarDate } from '../../shared/dates/calendar';
import { routePoint } from './transportation';
import { RegistryRoute } from '../routes/RegistryRoute';
import type { ReactNode } from 'react';
import { ownership } from '../routes/logisticsLabels';
import { type Transportation } from './types';
function MissingReference({ unavailable }: { unavailable?: boolean }) {
  const { translate } = useLocalization();
  return (
    <span className="registry-muted">{unavailable ? translate('registry.notLoaded') : '—'}</span>
  );
}

export const transportationColumns: RegistryColumn<Transportation>[] = [
  {
    id: 'container_number',
    labelKey: 'entity.transportation',
    filterFields: ['container_number', 'type', 'hazardous'],
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
    id: 'shipment_number',
    labelKey: 'registryField.shipmentNumber',
    filterFields: ['shipment_id'],
    width: 160,
    render: (item) =>
      item.shipment_number ? (
        <a
          className="link"
          href={`#/shipments/${item.shipment_id}`}
          onClick={(event) => event.stopPropagation()}
        >
          {item.shipment_number}
        </a>
      ) : (
        <MissingReference unavailable={item.shipment_unavailable} />
      ),
  },
  {
    id: 'customer_name',
    labelKey: 'field.customer',
    filterFields: ['customer_id'],
    width: 220,
    render: (item) =>
      item.customer_name || <MissingReference unavailable={item.customer_unavailable} />,
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
  const { translate } = useLocalization();
  const routeDetail = (stage: Transportation['route_segments'][number]) => {
    if (stage.mode === 'sea')
      return stage.sea_unloading_date
        ? translate('transport.seaUnloadedOn', { date: calendarDate(stage.sea_unloading_date) })
        : translate('transport.seaUnloadingMissing');
    if (stage.mode !== 'international_road') return undefined;

    const road = stage.international_road;
    if (road?.customs_departure_date)
      return translate('transport.customsDepartedOn', {
        date: calendarDate(road.customs_departure_date),
      });
    if (road?.customs_arrival_date)
      return translate('transport.customsArrivedOn', {
        date: calendarDate(road.customs_arrival_date),
      });
  };
  return (
    <RegistryRoute
      segments={item.route_segments.map((stage) => {
        const missing = translate('routeFilter.pointNotSet');
        const origin = routePoint(stage, 'origin').name || stage.plan?.origin_name || missing;
        const destination =
          routePoint(stage, 'destination').name || stage.plan?.destination_name || missing;
        return {
          id: stage.id,
          type: stage.mode,
          detail: routeDetail(stage),
          direction: `${origin} → ${destination}`,
        };
      })}
    />
  );
}

export function TransportationTable({
  items,
  pageSource,
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
  pageSource?: RegistryPageSource<Transportation>;
  sort?: RegistrySort;
  descending?: boolean;
  onSort?: (sort: RegistrySort) => void;
  columns?: RegistryColumns;
}) {
  const fallback = useRegistryColumns('', 'transportations', transportationColumns);
  return (
    <RegistryTable
      items={items}
      pageSource={pageSource}
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

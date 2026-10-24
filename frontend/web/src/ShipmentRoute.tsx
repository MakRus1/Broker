import { useRouteCollapse } from './routeCollapse';
import { Select } from './Select';
import {
  SharedRouteFields,
  SharedRouteDetailsView,
  sharedRouteDirection,
} from './SharedRouteDetails';
import type { Session, ShipmentRouteSegment } from './api';
import type { Counterparty } from './CustomerSelect';

type RouteType = { code: string; name: string };
export function ShipmentRoute({
  segments,
  onChange,
  allowedTypes,
  types,
  session,
  customers,
}: {
  segments: ShipmentRouteSegment[];
  onChange?: (segments: ShipmentRouteSegment[]) => void;
  allowedTypes: string[];
  types: RouteType[];
  session: Session;
  customers: Counterparty[];
}) {
  const collapse = useRouteCollapse(segments.map((stage) => stage.id));
  const title = (type: string) => types.find((item) => item.code === type)?.name ?? type;
  const update = (index: number, value: ShipmentRouteSegment) =>
    onChange?.(segments.map((old, i) => (i === index ? value : old)));
  function move(index: number, delta: number) {
    const next = [...segments];
    [next[index], next[index + delta]] = [next[index + delta], next[index]];
    onChange?.(next);
  }
  return (
    <section className="shipment-route-workspace">
      <div className="shipment-route-heading">
        <div>
          <h2>Маршрут поставки</h2>
          <p>Море и авиа — общие данные для всех перевозок.</p>
        </div>
      </div>
      {!!segments.length && (
        <div className="route-collapse-actions">
          <button type="button" onClick={collapse.expandAll}>
            Развернуть все
          </button>
          <button type="button" onClick={collapse.collapseAll}>
            Свернуть все
          </button>
        </div>
      )}
      <div className="shipment-route-stages">
        {segments.map((segment, index) => (
          <details
            className="route-segment shipment-route-stage"
            key={segment.id}
            open={!collapse.collapsed.has(segment.id)}
            onToggle={(event) => collapse.toggle(segment.id, !event.currentTarget.open)}
          >
            <summary className="route-stage-summary">
              <h3>
                {index + 1}. {title(segment.type)}
              </h3>
              <span className="route-summary-meta">
                {segment.type === 'sea' || segment.type === 'air'
                  ? sharedRouteDirection(segment)
                  : 'Данные заполняются в перевозке'}
              </span>
            </summary>
            {onChange ? (
              <>
                <div className="shipment-route-stage-toolbar">
                  <Select
                    label="Тип участка"
                    value={segment.type}
                    options={[...new Set([...allowedTypes, segment.type])].map((value) => ({
                      value,
                      label: title(value),
                    }))}
                    onChange={(type) => update(index, { id: segment.id, type })}
                  />
                  <div
                    className="shipment-route-actions"
                    role="group"
                    aria-label={`Действия с участком ${index + 1}`}
                  >
                    <button
                      type="button"
                      disabled={index === 0}
                      aria-label={`Поднять участок ${index + 1}`}
                      onClick={() => move(index, -1)}
                    >
                      ↑
                    </button>
                    <button
                      type="button"
                      disabled={index === segments.length - 1}
                      aria-label={`Опустить участок ${index + 1}`}
                      onClick={() => move(index, 1)}
                    >
                      ↓
                    </button>
                    <button
                      type="button"
                      onClick={() => onChange(segments.filter((_, i) => i !== index))}
                    >
                      Убрать
                    </button>
                  </div>
                </div>
                {segment.type === 'sea' || segment.type === 'air' ? (
                  <SharedRouteFields
                    type={segment.type}
                    sea={segment.sea}
                    air={segment.air}
                    session={session}
                    customers={customers}
                    onSea={(sea) =>
                      update(index, {
                        ...segment,
                        sea: Object.values(sea).some((value) => value !== undefined)
                          ? sea
                          : undefined,
                      })
                    }
                    onAir={(air) =>
                      update(index, {
                        ...segment,
                        air: Object.values(air).some((value) => value !== undefined)
                          ? air
                          : undefined,
                      })
                    }
                  />
                ) : (
                  <p className="muted">
                    Поля этого участка заполняются отдельно в каждой перевозке.
                  </p>
                )}
              </>
            ) : segment.type === 'sea' || segment.type === 'air' ? (
              <SharedRouteDetailsView
                sea={segment.sea}
                air={segment.air}
                customers={customers}
                session={session}
              />
            ) : (
              <p className="muted">Данные этого участка заполняются в перевозках.</p>
            )}
          </details>
        ))}
      </div>
      {!segments.length && <p className="muted">Добавьте участки в порядке следования.</p>}
      {onChange && (
        <button
          className="shipment-route-add"
          type="button"
          disabled={!allowedTypes.length || segments.length >= 30}
          onClick={() => {
            const id = crypto.randomUUID();
            onChange([...segments, { id, type: allowedTypes[0] }]);
            collapse.toggle(id, false);
          }}
        >
          ＋ Добавить участок
        </button>
      )}
    </section>
  );
}

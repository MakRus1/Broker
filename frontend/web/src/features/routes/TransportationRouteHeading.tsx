import { useLocalization } from '../localization/Localization';

export function TransportationRouteHeading({
  showTitle = true,
  railHint = false,
  shipmentId,
  onExpand,
  onCollapse,
}: {
  showTitle?: boolean;
  railHint?: boolean;
  shipmentId?: string;
  onExpand?: () => void;
  onCollapse?: () => void;
}) {
  const { translate } = useLocalization();
  return (
    <div className="route-view-heading">
      {showTitle && (
        <div className="section-heading">
          <div>
            <h2>{translate('forms.routeLegs')}</h2>
            {railHint && <p className="hint">{translate('forms.railLegDatesDoNotIncludeATime')}</p>}
          </div>
        </div>
      )}
      {onExpand && onCollapse && (
        <div className="route-collapse-actions">
          {shipmentId && (
            <a href={`#/shipments/${shipmentId}/route`}>{translate('forms.openShipmentRoute')}</a>
          )}
          <button type="button" onClick={onExpand}>
            {translate('route.expandAll')}
          </button>
          <button type="button" onClick={onCollapse}>
            {translate('route.collapseAll')}
          </button>
        </div>
      )}
    </div>
  );
}

import { useLocalization } from '../localization/Localization';
import { registryValueLabel } from '../registry/registryLabels';
import { RouteIcon } from '../routes/RouteIcon';
import './registryRoute.css';

export interface RegistryRouteLeg {
  id: string;
  type: string;
  direction: string;
  detail?: string;
}

export function RegistryRoute({ segments = [] }: { segments?: RegistryRouteLeg[] }) {
  const { locale, translate } = useLocalization();
  if (!segments.length)
    return <span className="registry-muted">{translate('routeFilter.notSet')}</span>;

  const remaining = segments.length - 1;
  const plural = new Intl.PluralRules(locale).select(remaining);
  const moreLabel =
    plural === 'one'
      ? 'shipment.moreRouteSegmentOne'
      : plural === 'few'
        ? 'shipment.moreRouteSegmentFew'
        : 'shipment.moreRouteSegmentMany';

  const segmentView = (segment: RegistryRouteLeg) => (
    <div className="registry-route-leg">
      <RouteIcon type={segment.type} />
      <div>
        <span>{registryValueLabel('mode', segment.type, locale) ?? segment.type}</span>
        <small>{segment.direction}</small>
        {segment.detail && <small>{segment.detail}</small>}
      </div>
    </div>
  );

  return (
    <div className="registry-route" onClick={(event) => event.stopPropagation()}>
      {segmentView(segments[0])}
      {segments.length > 1 && (
        <details>
          <summary>{translate(moreLabel, { count: remaining })}</summary>
          <ol>
            {segments.slice(1).map((segment) => (
              <li key={segment.id}>{segmentView(segment)}</li>
            ))}
          </ol>
        </details>
      )}
    </div>
  );
}

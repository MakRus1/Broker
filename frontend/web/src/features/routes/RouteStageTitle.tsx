import { RouteIcon } from './RouteIcon';
import { useLocalization } from '../localization/Localization';
import './routeStage.css';

export function RouteStageTitle({
  type,
  title,
  index,
}: {
  type: string;
  title: string;
  index: number;
}) {
  const { translate } = useLocalization();
  return (
    <h3 className="route-stage-title">
      <RouteIcon type={type} />
      <span className="route-stage-caption">
        <small aria-label={translate('route.segmentNumber', { number: index + 1 })}>
          {index + 1}.
        </small>
        <span>{title}</span>
      </span>
    </h3>
  );
}

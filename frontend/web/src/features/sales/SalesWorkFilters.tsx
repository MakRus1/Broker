import { useEffect, useState } from 'react';
import type { Session } from '../../shared/api/session';
import type { ColumnFilter } from '../registry/registryFilters';
import { useLocalization } from '../localization/Localization';
import { hasWorkFilter, salesWorkFilters } from './salesWorkFilterState';
import './salesWorkFilters.css';

export function SalesWorkFilters({
  session,
  filters,
  onToggle,
}: {
  session: Session;
  filters?: ColumnFilter[];
  onToggle: (filter: ColumnFilter) => void;
}) {
  const { translate } = useLocalization();
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const timer = window.setInterval(() => setNow(new Date()), 60000);
    return () => window.clearInterval(timer);
  }, []);
  return (
    <div className="sales-work-filters" role="group" aria-label={translate('sales.workFilters')}>
      {salesWorkFilters(session.subject, session.display_name, now).map((option) => (
        <button
          type="button"
          key={option.id}
          aria-pressed={hasWorkFilter(filters, option.filter)}
          onClick={() => onToggle(option.filter)}
        >
          {translate(`sales.quick.${option.id}`)}
        </button>
      ))}
    </div>
  );
}

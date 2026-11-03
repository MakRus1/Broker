import { useLocalization } from '../../features/localization/Localization';
import { useState, type ReactNode } from 'react';
export function CardNavigation({
  children,
  onRefresh,
  disabled = false,
  className = '',
  tabsClassName = '',
}: {
  children: ReactNode;
  onRefresh?: () => Promise<unknown>;
  disabled?: boolean;
  className?: string;
  tabsClassName?: string;
}) {
  const { translate } = useLocalization();
  const [refreshing, setRefreshing] = useState(false);
  return (
    <div className={`card-navigation ${className}`}>
      <div className={`tabs ${tabsClassName}`}>{children}</div>
      {onRefresh && (
        <button
          type="button"
          className="card-refresh"
          disabled={disabled || refreshing}
          onClick={async () => {
            setRefreshing(true);
            try {
              await onRefresh();
            } finally {
              setRefreshing(false);
            }
          }}
        >
          <span aria-hidden="true">↻</span>{' '}
          {refreshing ? translate('ui.refreshing') : translate('action.refresh')}
        </button>
      )}
    </div>
  );
}
